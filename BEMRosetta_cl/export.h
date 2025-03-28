// SPDX-License-Identifier: GPL-3.0-or-later
// Copyright 2020 - 2022, the BEMRosetta author and contributors
extern "C" {
	__declspec(dllexport) double DemoVectorPy_C(const double *v, int num) noexcept; 	
	
	// Returns dll time and date
	__declspec(dllexport) const char *DLL_Version() noexcept; 		
	// Lists all available functions
	__declspec(dllexport) void DLL_ListFunctions() noexcept;
	// Returns the list of all available functions
	__declspec(dllexport) const char *DLL_strListFunctions() noexcept;		
	// Returns the Python declaration of all available functions
	__declspec(dllexport) const char *DLL_strPythonDeclaration() noexcept;	
	
	// Blocks the printing of messages on the screen
	__declspec(dllexport) void DLL_NoPrint() noexcept;	
	
	// Loads a FAST .out or .outb file
	__declspec(dllexport) int DLL_FAST_Load(const char *filename) noexcept;		
	// Returns the parameter name of index id
	__declspec(dllexport) const char *DLL_FAST_GetParameterName(int id) noexcept;
	// Returns the parameter units of index id
	__declspec(dllexport) const char *DLL_FAST_GetUnitName(int id) noexcept;
	// Returns the index id of parameter name
	__declspec(dllexport) int DLL_FAST_GetParameterId(const char *name) noexcept;		
	// Returns the number of parameters
	__declspec(dllexport) int DLL_FAST_GetParameterCount() noexcept;				
	// Returns the number of registers per parameter
	__declspec(dllexport) int DLL_FAST_GetLen() noexcept;			
	// Returns the initial time in seconds
	__declspec(dllexport) double DLL_FAST_GetTimeStart() noexcept;	
	// Returns the end time in seconds
	__declspec(dllexport) double DLL_FAST_GetTimeEnd() noexcept;	
	// Returns the idtime_th time (idtime goes from 0 to DLL_FAST_GetLen())
	__declspec(dllexport) double DLL_FAST_GetTime(int idtime) noexcept;
	// Returns the idtime_th value of parameter idparam (idtime goes from 0 to DLL_FAST_GetLen())
	__declspec(dllexport) double DLL_FAST_GetData(int idtime, int idparam) noexcept;
	// Returns the average value for parameter idparam
	__declspec(dllexport) double DLL_FAST_GetAvg(int idparam, int idbegin, int idend) noexcept;
	// Returns the maximum value for parameter idparam
	__declspec(dllexport) double DLL_FAST_GetMax(int idparam, int idbegin, int idend) noexcept;
	// Returns the minimum value for parameter idparam
	__declspec(dllexport) double DLL_FAST_GetMin(int idparam, int idbegin, int idend) noexcept;
	// Returns an Array of parameter idparam 
	__declspec(dllexport) int DLL_FAST_GetArray(int idparam, int idbegin, int idend, double **data, int *num) noexcept;
	
	// Open a .dat or .fst FAST file to read or save parameters
	__declspec(dllexport) int DLL_FAST_LoadFile(const char *file) noexcept;
	// Saves the .dat or .fst FAST file opened with FAST_LoadFile() (if file is ""), or to the file indicated in file
	__declspec(dllexport) int DLL_FAST_SaveFile(const char *file) noexcept;
	// Sets the value of a var after paragraph. If paragraph is "", the value is set every time var appears in the file
	__declspec(dllexport) int DLL_FAST_SetVar(const char *name, const char *paragraph, const char *value) noexcept;
	// Reads the value of a var after paragraph. If paragraph is "", it is read the first time var appears in the file
	__declspec(dllexport) const char *DLL_FAST_GetVar(const char *name, const char *paragraph) noexcept;
};
