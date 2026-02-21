#pragma once

#ifndef __SDKDLLEXPORT_H__
#define __SDKDLLEXPORT_H__

#ifdef __cplusplus
extern "C" {
#endif 

#ifdef LIVECLOUDKDSDK_EXPORTS
	#define DLLEXPORT __declspec(dllexport)
	#define FUNCTION_TYPE DLLEXPORT
#else
#define FUNCTION_TYPE DECLSPEC_IMPORT
#endif

#ifdef __cplusplus
};
#endif

#endif //#ifndef __SDKDLLEXPORT_H__