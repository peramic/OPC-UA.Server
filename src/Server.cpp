#include "Server.h"
#include "ServerException.h"
#include "OpcUaServerCore.h" // OpcUaServerCore
#include "logging/ServerSdkLoggerFactory.h"
#include "utilities/linux.h" // RegisterSignalHandler
#include <common/Exception.h> // ExceptionDef
#include <common/logging/Logger.h>
#include <common/logging/LoggerFactory.h>
#include <ioDataProvider/IODataProviderException.h>
#include <ioDataProvider/IODataProvider.h>
#include <sasModelProvider/SASModelProvider.h>
#include <sasModelProvider/SASModelProviderException.h>
#include <ualocalizedtext.h> // UaLocalizedText
#include <uaplatformlayer.h> // UaPlatformLayer
#include <uaserverapplication.h> // UaServerApplicationModule
#include <uastring.h> // UaString
#include <xmldocument.h> // UaXmlDocument
#include <algorithm>  // std::sort
#include <dirent.h> // opendir
#include <dlfcn.h> // dlopen
#include <sstream> // std::ostringstream
#include <vector>
#include <config.h>  // SERVER_VERSION (generated by cmake)

using namespace CommonNamespace;

class ServerPrivate {
    friend class Server;
private:
    typedef void destroyIODataProviderFactory_t(IODataProviderNamespace::IODataProviderFactory*);
    typedef void destroySASModelProviderFactory_t(SASModelProviderNamespace::SASModelProviderFactory*);

    Logger* log;
    LoggerFactory* loggerFactory;

    OpcUa_UInt shutdownDelay;
    char* appPath;
    OpcUaServerCore* opcUaServerCore;

    std::vector<void *> libHandles;
    std::map<IODataProviderNamespace::IODataProviderFactory*, destroyIODataProviderFactory_t*> ioDataProviderFactories;
    std::vector<IODataProviderNamespace::IODataProvider*> ioDataProviders;
    std::map<SASModelProviderNamespace::SASModelProviderFactory*, destroySASModelProviderFactory_t*> sasModelProviderFactories;
    std::vector<SASModelProviderNamespace::SASModelProvider*> sasModelProviders;
};

Server::Server(OpcUa_UInt shutdownDelay) {
    d = new ServerPrivate();
    d->log = LoggerFactory::getLogger("Server");
    d->loggerFactory = NULL;
    d->log->info("Starting server %s for %s", SERVER_VERSION, ARCH);
    registerSignalHandler();
    d->appPath = getAppPath();
    d->shutdownDelay = shutdownDelay;
}

Server::~Server() {
    close();
    delete[] d->appPath;
    delete d;
}

void Server::open()/*throws ServerException, IODataProviderException, SASModelProviderException*/ {
    try {
        // initialize the XML parser
        UaXmlDocument::initParser();
        // initialize the UA Stack platform layer    
        if (UaPlatformLayer::init() != 0) {
            std::ostringstream msg;
            msg << "Cannot initialize UA stack platform layer";
            throw ExceptionDef(ServerException, msg.str());
        }

        UaString serverConfDir(d->appPath);
        serverConfDir += "/conf/";
        UaString providerConfDir = serverConfDir + "provider/";

        // create and initialize server        
        d->opcUaServerCore = new OpcUaServerCore();
        UaString serverConfFile(serverConfDir + "ServerConfig.xml");
        UaString appPath(d->appPath);
        d->opcUaServerCore->setServerConfig(serverConfFile, appPath);

        // replace default logger factory with factory for client SDK
        Logger* oldLog = d->log;
        d->loggerFactory = new LoggerFactory(*new ServerSdkLoggerFactory(), true /*attachValues*/);
        d->log = LoggerFactory::getLogger("Server");

        // start the core server
        if (d->opcUaServerCore->start() != 0) {
            throw ExceptionDef(ServerException, std::string("Cannot start server"));
        }
        OpcUa_Boolean traceEnabled;
        OpcUa_UInt32 traceLevel;
        OpcUa_UInt32 maxTraceEntries;
        OpcUa_UInt32 maxBackupFiles;
        UaString traceFile;
        OpcUa_Boolean disableFlush;
        d->opcUaServerCore->getServerConfig()->getServerTraceSettings(traceEnabled,
                traceLevel, maxTraceEntries, maxBackupFiles, traceFile, disableFlush);
        oldLog->info("Activated logging to %s", traceFile.toUtf8());

        // open libs
        std::string providerLibDir(d->appPath);
        providerLibDir += "/lib/provider/";

        std::string libPrefix("lib");
        std::string libSuffix;
#ifdef DEBUG
        libSuffix = DEBUG_POSTFIX;
#endif
        libSuffix += ".so";
        int libPrefixLength = libPrefix.length();
        int libSuffixLength = libSuffix.length();

        // get directory entries of providerLibDir and sort them
        std::vector<std::string> dirEntries;
        DIR* dir;
        dirent* dirEntry;
        if ((dir = opendir(providerLibDir.c_str())) != NULL) {
            /* get all files and directories within directory */
            while ((dirEntry = readdir(dir)) != NULL) {
                dirEntries.push_back(std::string(dirEntry->d_name));
            }
            closedir(dir);
        } else {
            std::ostringstream msg;
            msg << "Cannot open directory " << providerLibDir.c_str();
            throw ExceptionDef(ServerException, msg.str());
        }
        std::sort(dirEntries.begin(), dirEntries.end());

        typedef IODataProviderNamespace::IODataProviderFactory * createIODataProviderFactory_t();
        typedef SASModelProviderNamespace::SASModelProviderFactory * createSASModelProviderFactory_t();

        std::map<std::string, IODataProviderNamespace::IODataProviderFactory*> ioDataProviderFactories;
        std::map<std::string, SASModelProviderNamespace::SASModelProviderFactory*> sasModelProviderFactories;
        // for each directory entry of providerLibDir
        for (std::vector<std::string>::const_iterator i = dirEntries.begin();
                i != dirEntries.end(); i++) {
            std::string dirEntry = *i;
            // get module name from directory entry:
            // prefix "lib", suffix ".so", dirEntry "libmy.so" -> my
            // prefix "lib", suffix "d.so", dirEntry "01_libmyd.so" -> my
            std::string moduleName;
            int countNumber = dirEntry.find_first_not_of("0123456789_", 0 /*pos*/);
            if (dirEntry.length() > countNumber + libPrefixLength + libSuffixLength) {
                moduleName = dirEntry.substr(countNumber + libPrefixLength,
                        dirEntry.length() - countNumber - libPrefixLength - libSuffixLength);
            } else {
                // entry is not a provider library like "." or ".."
                continue;
            }
            // open lib
            std::string libFileStr(providerLibDir);
            libFileStr += dirEntry;
            const char* libFile = libFileStr.c_str();
            void* libHandle = dlopen(libFile, RTLD_NOW);
            if (libHandle == NULL) {
                d->log->error("Cannot open library %s: %s", libFile, dlerror());
                continue;
            }
            d->libHandles.push_back(libHandle);

            createIODataProviderFactory_t* createIODataProviderFactoryDlsym
                    = (createIODataProviderFactory_t*) dlsym(libHandle, "createIODataProviderFactory");
            if (createIODataProviderFactoryDlsym != NULL) {
                if (ioDataProviderFactories.size() == 0) {
                    d->log->info("Found IO data provider factory in %s", libFile);
                    // create factory
                    IODataProviderNamespace::IODataProviderFactory* providerFactory = createIODataProviderFactoryDlsym();
                    ioDataProviderFactories[moduleName] = providerFactory;
                    ServerPrivate::destroyIODataProviderFactory_t* destroyIODataProviderFactoryDlsym
                            = (ServerPrivate::destroyIODataProviderFactory_t*) dlsym(libHandle, "destroyIODataProviderFactory");
                    d->ioDataProviderFactories[providerFactory] = destroyIODataProviderFactoryDlsym;
                    ;
                } else {
                    d->log->info("Ignoring IO data provider factory in %s", libFile);
                }
            }

            createSASModelProviderFactory_t* createSASModelProviderFactoryDlsym
                    = (createSASModelProviderFactory_t*) dlsym(libHandle, "createSASModelProviderFactory");
            if (createSASModelProviderFactoryDlsym != NULL) {
                if (sasModelProviderFactories.size() == 0) {
                    d->log->info("Found SAS model provider factory in %s", libFile);
                    // create factory
                    SASModelProviderNamespace::SASModelProviderFactory* providerFactory =
                            createSASModelProviderFactoryDlsym();
                    sasModelProviderFactories[moduleName] = providerFactory;
                    ServerPrivate::destroySASModelProviderFactory_t* destroySASModelProviderFactoryDlsym
                            = (ServerPrivate::destroySASModelProviderFactory_t*) dlsym(libHandle,
                            "destroySASModelProviderFactory");
                    d->sasModelProviderFactories[providerFactory] = destroySASModelProviderFactoryDlsym;
                } else {
                    d->log->info("Ignoring SAS model provider factory in %s", libFile);
                }
            }
        }

        // open IO data provider
        for (std::map<std::string, IODataProviderNamespace::IODataProviderFactory*>::iterator i
                = ioDataProviderFactories.begin(); i != ioDataProviderFactories.end(); i++) {
            std::string moduleName = (*i).first;
            IODataProviderNamespace::IODataProviderFactory& providerFactory = *(*i).second;
            // create providers
            std::vector<IODataProviderNamespace::IODataProvider*>& providers =
                    *providerFactory.create();
            // open providers
            std::string moduleConfDir(providerConfDir.toUtf8());
            moduleConfDir += moduleName + '/';
            std::vector<IODataProviderNamespace::IODataProvider*>::iterator iter;
            try {
                for (iter = providers.begin(); iter != providers.end(); iter++) {
                    (*iter)->open(moduleConfDir); // IODataProviderException
                    //TODO handle all providers
                    iter++;
                    break;
                }
                d->ioDataProviders.insert(d->ioDataProviders.end(), providers.begin(), iter);
            } catch (IODataProviderNamespace::IODataProviderException& e) {
                d->ioDataProviders.insert(d->ioDataProviders.end(), providers.begin(), iter);
                throw e;
            }
        }
        if (d->ioDataProviders.size() == 0) {
            std::ostringstream msg;
            msg << "No IO data provider library found in " << providerLibDir.c_str();
            throw ExceptionDef(ServerException, msg.str());
        }
        //TODO create group of IO data providers:
        //       class IODataProviderGroup implementing IODataProvider interface
        IODataProviderNamespace::IODataProvider* ioDataProvider = d->ioDataProviders.at(0);

        // open SAS model provider
        for (std::map<std::string, SASModelProviderNamespace::SASModelProviderFactory*>::iterator i
                = sasModelProviderFactories.begin(); i != sasModelProviderFactories.end(); i++) {
            std::string moduleName = (*i).first;
            SASModelProviderNamespace::SASModelProviderFactory& providerFactory = *(*i).second;
            // create providers
            std::vector<SASModelProviderNamespace::SASModelProvider*>& providers =
                    *providerFactory.create();
            // open providers
            std::string moduleConfDir(providerConfDir.toUtf8());
            moduleConfDir += moduleName + '/';
            std::vector<SASModelProviderNamespace::SASModelProvider*>::iterator iter;
            try {
                for (iter = providers.begin(); iter != providers.end(); iter++) {
                    (*iter)->open(moduleConfDir, *ioDataProvider); // SASModelProviderException
                    //TODO handle all providers
                    iter++;
                    break;
                }
                d->sasModelProviders.insert(d->sasModelProviders.end(), providers.begin(), iter);
            } catch (SASModelProviderNamespace::SASModelProviderException& e) {
                d->sasModelProviders.insert(d->sasModelProviders.end(), providers.begin(), iter);
                throw e;
            }
        }
        if (d->sasModelProviders.size() == 0) {
            std::ostringstream msg;
            msg << "No SAS model provider library found in " << providerLibDir.c_str();
            throw ExceptionDef(ServerException, msg.str());
        }
        //TODO create group of SAS model providers:
        //        class SASModelProviderGroup implementing SASModelProvider interface
        SASModelProviderNamespace::SASModelProvider* sasModelProvider = d->sasModelProviders.at(0);

        // add SAS model providers to server
        std::vector<NodeManager*>* nodeManagers = sasModelProvider->createNodeManagers();
        if (nodeManagers != NULL) {
            for (int i = 0; i < nodeManagers->size(); i++) {
                d->opcUaServerCore->addNodeManager((*nodeManagers)[i]);
            }
            delete nodeManagers;
        }
        std::vector<UaServerApplicationModule*>* serverApplicationModules =
                sasModelProvider->createServerApplicationModules();
        if (serverApplicationModules != NULL) {
            for (int i = 0; i < serverApplicationModules->size(); i++) {
                d->opcUaServerCore->addModule((*serverApplicationModules)[i]);
            }
            delete serverApplicationModules;
        }
    } catch (CommonNamespace::Exception& e) {
        close();
        throw;
    }
}

void Server::close() {
    if (d->opcUaServerCore != NULL) {
        if (d->opcUaServerCore->isStarted()) {
            // stop the server and wait some time if clients are connected
            // to allow them to disconnect after they received the shutdown signal
            d->opcUaServerCore->stop(d->shutdownDelay, UaLocalizedText("", "User shutdown"));
        }
        delete d->opcUaServerCore;
        d->opcUaServerCore = NULL;
    }
    // close and delete SAS model providers
    for (std::vector<SASModelProviderNamespace::SASModelProvider*>::iterator i =
            d->sasModelProviders.begin(); i != d->sasModelProviders.end(); i++) {
        (*i)->close();
        delete *i;
    }
    d->sasModelProviders.clear();
    // destroy SAS model provider factories
    for (std::map<SASModelProviderNamespace::SASModelProviderFactory*,
            ServerPrivate::destroySASModelProviderFactory_t*>::iterator i =
            d->sasModelProviderFactories.begin(); i != d->sasModelProviderFactories.end(); i++) {
        SASModelProviderNamespace::SASModelProviderFactory* factory = (*i).first;
        ServerPrivate::destroySASModelProviderFactory_t* destroy = (*i).second;
        destroy(factory);
    }
    d->sasModelProviderFactories.clear();

    // close and delete IO data providers
    for (std::vector<IODataProviderNamespace::IODataProvider*>::iterator i =
            d->ioDataProviders.begin(); i != d->ioDataProviders.end(); i++) {
        (*i)->close();
        delete *i;
    }
    d->ioDataProviders.clear();
    // destroy IO data provider factories
    for (std::map<IODataProviderNamespace::IODataProviderFactory*,
            ServerPrivate::destroyIODataProviderFactory_t*>::iterator i =
            d->ioDataProviderFactories.begin(); i != d->ioDataProviderFactories.end(); i++) {
        IODataProviderNamespace::IODataProviderFactory* factory = (*i).first;
        ServerPrivate::destroyIODataProviderFactory_t* destroy = (*i).second;
        destroy(factory);
    }
    d->ioDataProviderFactories.clear();

    // close libs
    for (std::vector<void*>::iterator i = d->libHandles.begin(); i != d->libHandles.end(); i++) {
        dlclose(*i);
    }
    d->libHandles.clear();

    delete d->loggerFactory;
    d->loggerFactory = NULL;

    // clean up the UA Stack platform layer
    UaPlatformLayer::cleanup();
    // clean up the XML parser
    UaXmlDocument::cleanupParser();
}