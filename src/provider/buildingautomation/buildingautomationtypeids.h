/******************************************************************************
 ** Copyright (C) 2006-2008 Unified Automation GmbH. All Rights Reserved.
 ** Web: http://www.unifiedautomation.com
 ** This file is provided AS IS with NO WARRANTY OF ANY KIND, INCLUDING THE
 ** WARRANTY OF DESIGN, MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE.
 **
 ** Project: Sample OPC server
 **
 ** Description: Numeric Id definitions for building automation type nodes.
 ******************************************************************************/

/************************************************************
 DataTypes
 *************************************************************/
#define Ba_ControllerConfiguration                         2
#define Ba_ControllerConfiguration_DefaultBinary           3
#define Ba_ControllerConfiguration_DefaultXml              4
#define Ba_AirConditionerControllerConfiguration           5
#define Ba_AirConditionerControllerConfiguration_DefaultBinary 6
#define Ba_AirConditionerControllerConfiguration_DefaultXml    7
#define Ba_Configurations                                  8
#define Ba_Configurations_DefaultBinary                    9
#define Ba_Configurations_DefaultXml                      10

/************************************************************
 Controller Type and its instance declaration
 *************************************************************/
// Controller Type
#define Ba_ControllerType                                1000
// Instance declaration
#define Ba_ControllerType_State                          1001
#define Ba_ControllerType_Temperature                    1002
#define Ba_ControllerType_TemperatureSetpoint            1003
#define Ba_ControllerType_PowerConsumption               1004
#define Ba_ControllerType_Start                          1006
#define Ba_ControllerType_Stop                           1007
#define Ba_ControllerType_Temperature_EURange            1008
#define Ba_ControllerType_Temperature_EngineeringUnits   1009
#define Ba_ControllerType_TemperatureSetpoint_EURange    1010
#define Ba_ControllerType_TemperatureSetpoint_EngineeringUnits 1011
#define Ba_ControllerType_SetControllerConfigurations    1012
#define Ba_ControllerType_SetControllerConfigurations_In 1013
#define Ba_ControllerType_SetControllerConfigurations_Out 1014
/************************************************************/

/************************************************************
 AirConditioner Controller Type and its instance declaration
 *************************************************************/
// AirConditioner Controller Type
#define Ba_AirConditionerControllerType                  2000
// Instance declaration
//#define Ba_AirConditionerControllerType_State            2001
#define Ba_AirConditionerControllerType_Humidity         2002
#define Ba_AirConditionerControllerType_HumiditySetpoint 2003
#define Ba_AirConditionerControllerType_StartWithSetpoint 2004
#define Ba_AirConditionerControllerType_StartWithSetpoint_In 2005
/************************************************************/

/************************************************************
 Furnace Controller Type and its instance declaration
 *************************************************************/
// Furnace Controller Type
#define Ba_FurnaceControllerType                         3000
// Instance declaration
#define Ba_FurnaceControllerType_State                   3001
#define Ba_FurnaceControllerType_GasFlow                 3002
#define Ba_FurnaceControllerType_StartWithSetpoint       3003
#define Ba_FurnaceControllerType_StartWithSetpoint_In    3004
/************************************************************/

/************************************************************
 ControllerEventType and its event field properties
 *************************************************************/
// ControllerEventType
#define Ba_ControllerEventType                           4000
// Event field properties
#define Ba_ControllerEventType_ControllerConfigurations  4001
#define Ba_ValuesControllerEventType                     4010
#define Ba_ValuesControllerEventType_Temperature         4011
#define Ba_ValuesControllerEventType_State               4012
/************************************************************/
