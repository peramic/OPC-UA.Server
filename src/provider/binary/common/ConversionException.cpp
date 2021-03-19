#include "ConversionException.h"

using namespace CommonNamespace;

ConversionException::ConversionException(const std::string& message,
		const std::string& file, int line) :
		Exception(message, file, line) {
}

Exception* ConversionException::copy() const {
	return new ConversionException(*this);
}
