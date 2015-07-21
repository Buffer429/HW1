#include <stdio.h>
#include <malloc.h>
#include <string.h>
#include <Windows.h>
#include <strsafe.h>
#include <Shlwapi.h>
#include <stdint.h>
#include <errno.h>
#include <crtdbg.h>
#include <locale.h>



void print(_In_ const char* fmt, _In_ ...)
{
	char log_buffer[2048];
	va_list args;

	va_start(args, fmt);
	HRESULT hRes = StringCbVPrintfA(log_buffer, sizeof(log_buffer), fmt, args);
	if (S_OK != hRes) {
		fprintf(stderr, "%s, StringCbVPrintfA() failed. res = 0x%08x", __FUNCTION__, hRes);
		return;
	}

	OutputDebugStringA(log_buffer);
	fprintf(stdout, "%s \n", log_buffer);
}

bool read_file_using_memory_map()
{
	// current directory 를 구한다.
	wchar_t *buf = NULL;
	uint32_t buflen = 0;
	buflen = GetCurrentDirectoryW(buflen, buf);
	if (0 == buflen)
	{
		print("err, GetCurrentDirectoryW() failed. gle = 0x%08x", GetLastError());
		return false;
	}

	buf = (PWSTR)malloc(sizeof(WCHAR) * buflen);
	if (0 == GetCurrentDirectoryW(buflen, buf))
	{
		print("err, GetCurrentDirectoryW() failed. gle = 0x%08x", GetLastError());
		free(buf);
		return false;
	}

	// current dir \\ test.txt 파일명 생성
	wchar_t file_name[260];
	if (!SUCCEEDED(StringCbPrintfW(
		file_name,
		sizeof(file_name),
		L"%ws\\bob.txt",
		buf)))
	{
		print("err, can not create file name");
		free(buf);
		return false;
	}
	free(buf); buf = NULL;

	/*if (true == is_file_existsW(file_name))
	{
	::DeleteFileW(file_name);
	}*/




	HANDLE file_handle = CreateFileW((LPCWSTR)file_name, GENERIC_READ, NULL, NULL,OPEN_EXISTING,FILE_ATTRIBUTE_NORMAL,NULL);
	if (INVALID_HANDLE_VALUE == file_handle)
	{
		print("err, CreateFile(%ws) failed, gle = %u", file_name, GetLastError());
		return false;
	}

	// check file size
	//
	LARGE_INTEGER fileSize;
	if (TRUE != GetFileSizeEx(file_handle, &fileSize))
	{
		print("err, GetFileSizeEx(%ws) failed, gle = %u", file_name, GetLastError());
		CloseHandle(file_handle);
		return false;
	}

	// [ WARN ]
	//
	// 4Gb 이상의 파일의 경우 MapViewOfFile()에서 오류가 나거나
	// 파일 포인터 이동이 문제가 됨
	// FilIoHelperClass 모듈을 이용해야 함
	//
	_ASSERTE(fileSize.HighPart == 0);
	if (fileSize.HighPart > 0)
	{
		print("file size = %I64d (over 4GB) can not handle. use FileIoHelperClass",
			fileSize.QuadPart);
		CloseHandle(file_handle);
		return false;
	}

	DWORD file_size = (DWORD)fileSize.QuadPart;
	HANDLE file_map = CreateFileMapping(file_handle,NULL,PAGE_READONLY,0,0,NULL);
	if (NULL == file_map)
	{
		print("err, CreateFileMapping(%ws) failed, gle = %u", file_name, GetLastError());
		CloseHandle(file_handle);
		return false;
	}

	PCHAR file_view = (PCHAR)MapViewOfFile(file_map,FILE_MAP_READ,0,0,0);
	if (file_view == NULL)
	{
		print("err, MapViewOfFile(%ws) failed, gle = %u", file_name, GetLastError());

		CloseHandle(file_map);
		CloseHandle(file_handle);
		return false;
	}

	// do some io
	char a = file_view[0];  // 0x d9
	char b = file_view[1];  // 0xb3



	// close all
	UnmapViewOfFile(file_view);
	CloseHandle(file_map);
	CloseHandle(file_handle);
	return true;
}

wchar_t* MbsToWcs(_In_ const char* mbs)
{
	_ASSERTE(NULL != mbs);
	if (NULL == mbs) return NULL;

	int outLen = MultiByteToWideChar(CP_ACP, MB_PRECOMPOSED, mbs, -1, NULL, 0);
	if (0 == outLen) return NULL;

	wchar_t* outWchar = (wchar_t*)malloc(outLen * (sizeof(wchar_t)));  // outLen contains NULL char 

	if (NULL == outWchar) return NULL;
	RtlZeroMemory(outWchar, outLen);

	if (0 == MultiByteToWideChar(CP_ACP, MB_PRECOMPOSED, mbs, -1, outWchar, outLen)) {
		print("MultiByteToWideChar() failed, errcode=0x%08x", GetLastError());
		free(outWchar);
		return NULL;
	}

	return outWchar;
}

char* WcsToMbsUTF8(_In_ const wchar_t* wcs)
{
	_ASSERTE(NULL != wcs);
	if (NULL == wcs) return NULL;

	int outLen = WideCharToMultiByte(CP_UTF8, 0, wcs, -1, NULL, 0, NULL, NULL);
	if (0 == outLen) return NULL;

	char* outChar = (char*)malloc(outLen * sizeof(char));
	if (NULL == outChar) return NULL;
	RtlZeroMemory(outChar, outLen);

	if (0 == WideCharToMultiByte(CP_UTF8, 0, wcs, -1, outChar, outLen, NULL, NULL)) {
		print("WideCharToMultiByte() failed, errcode=0x%08x", GetLastError());
		free(outChar);
		return NULL;
	}

	return outChar;
}

wchar_t* Utf8MbsToWcs(_In_ const char* utf8)
{
	_ASSERTE(NULL != utf8);
	if (NULL == utf8) return NULL;

	int outLen = MultiByteToWideChar(CP_UTF8, MB_ERR_INVALID_CHARS, utf8, -1, NULL, 0);
	if (0 == outLen) return NULL;

	wchar_t* outWchar = (wchar_t*)malloc(outLen * (sizeof(wchar_t)));  // outLen contains NULL char 
	if (NULL == outWchar) return NULL;
	RtlZeroMemory(outWchar, outLen);

	if (0 == MultiByteToWideChar(CP_UTF8, MB_ERR_INVALID_CHARS, utf8, -1, outWchar, outLen)) {
		print("MultiByteToWideChar() failed, errcode=0x%08x", GetLastError());
		free(outWchar);
		return NULL;
	}

	return outWchar;
}

bool copy_bob()
{
	wchar_t *buf = NULL;
	uint32_t buflen = 0;
	buflen = GetCurrentDirectoryW(buflen, buf);

	if (buflen == 0)
	{
		print("err GetCurrentDirectoryW() failed gle = 0x%08x", GetLastError());
		return false;
	}

	buf = (PWSTR)malloc(sizeof(WCHAR)*buflen);
	if (GetCurrentDirectoryW(buflen, buf) == 0)
	{
		print("err GetCurrentDirectoryW() failed gle = 0x%08x", GetLastError());
		free(buf);
		return false;
	}


	wchar_t file_name[260];
	if (!SUCCEEDED(StringCbPrintfW(file_name, sizeof(file_name), L"%ws\\bob.txt", buf)))
	{
		printf("err can not create name %08x", GetLastError());
		return false;
	}

	wchar_t file_copy[260];
	if (!SUCCEEDED(StringCbPrintfW(file_copy, sizeof(file_copy), L"%ws\\bob2.txt", buf)))
	{
		printf("err can not create name %08x", GetLastError());
		return false;
	}

	free(buf);
	buf = NULL;
	//파일 복사

	CopyFile(file_name, file_copy, 0);

	return TRUE;
}

bool create_bob_txt()
{
	// current directory 를 구한다.
	wchar_t *buf = NULL;
	uint32_t buflen = 0;

	buflen = GetCurrentDirectoryW(buflen, buf);
	if (0 == buflen) {
		print("err, GetCurrentDirectoryW() failed. gle = 0x%08x", GetLastError());
		return false;
	}

	buf = (PWSTR)malloc(sizeof(WCHAR) * buflen);

	if (0 == GetCurrentDirectoryW(buflen, buf)) {
		print("err, GetCurrentDirectoryW() failed. gle = 0x%08x", GetLastError());
		free(buf);
		return false;
	}

	wchar_t file_name[260];           // bob1에서 생성한 파일을 읽을 준비.
	if (!SUCCEEDED(StringCbPrintfW(
		file_name,
		sizeof(file_name),
		L"%ws\\bob2.txt", buf)))   {
		print("err, can not create file name");
		free(buf);
		return false;
	}

	free(buf); buf = NULL;

	HANDLE file_handle = CreateFileW( // 메모리에 매핑할 파일의 핸들을 얻는다. 실패 시 INVALID_HANDLE_VALUE를 반환
		(LPCWSTR)file_name,
		GENERIC_READ,
		NULL,
		NULL,
		OPEN_EXISTING, //기존 파일
		FILE_ATTRIBUTE_NORMAL,
		NULL
		);

	if (INVALID_HANDLE_VALUE == file_handle) {
		print("err, CreateFile(%ws) failed, gle = %u", file_name, GetLastError());
		return false;
	}

	LARGE_INTEGER fileSize;
	if (TRUE != GetFileSizeEx(file_handle, &fileSize)) { // 매핑할 파일의 전체 크기를 구한다.
		print("err, GetFileSizeEx(%ws) failed, gle = %u", file_name, GetLastError());
		CloseHandle(file_handle);
		return false;
	}
	_ASSERTE(fileSize.HighPart == 0); // 식을 계산하고 결과가 false 일때, 진단 메시지를 출력하고 프로그램을 중단
	/*
	assert 매크로는 일반적으로 프로그램이 올바르게 작동하지 않을때 false 로 계산하기 위해 expression 인수를 구현함으로써
	프로그램을 개발하는 동안 논리적인 오류들을 식별하기 위해 사용된다.
	디버깅이 완전히 끝나면, NDEBUG 식별자를 정의하여 소스 파일을 수정하지 않고 어썰션 검사를 끌 수 있다.
	*/
	if (fileSize.HighPart > 0) {
		print("file size = %I64d (over 4GB) can not handle. use FileIoHelperClass",
			fileSize.QuadPart);
		CloseHandle(file_handle);
		return false;
	}

	DWORD file_size = (DWORD)fileSize.QuadPart;
	/*
	앞서 CreateFile()을 호출한 이유는 파일 매핑을 수행할 파일의 물리 저장소를 운영체제에게 알려주기 위해.
	매핑할 파일을 대상으로 파일 매핑 오브젝트를 생성한다.
	지정 파일을 파일 매핑 오브젝트와 연결하며, => reserve
	파일 매핑 오브젝트를 위한 충분한 물리 저장소가 존재한다는 것을 확인시키는 작업.
	실패시 NULL 반환
	*/
	HANDLE file_map = CreateFileMapping(
		file_handle,
		NULL,
		PAGE_READONLY,
		0,
		0,
		NULL
		);
	if (NULL == file_map) {
		print("err, CreateFileMapping(%ws) failed, gle = %u", file_name, GetLastError());
		CloseHandle(file_handle);
		return false;
	}

	// PCHAR : A pointer to a CHAR. This type is declared in WinNT.h as follows : typedef CHAR *PCHAR
	PCHAR file_view = (PCHAR)MapViewOfFile(
		file_map,
		FILE_MAP_READ,
		0,
		0,
		0
		);

	if (file_view == NULL) {
		print("err, MapViewOfFile(%ws) failed, gle = %u", file_name, GetLastError());

		CloseHandle(file_map);
		CloseHandle(file_handle);
		return false;
	}

	// UTF-8 -> UTF-16(UCS-2 지원)
	wchar_t *convchar5 = Utf8MbsToWcs(file_view);
	_wsetlocale(LC_ALL, L"korean");
	wprintf(L"%s\n", convchar5); // 뷰로부터 읽은 데이터를 콘솔에 출력.

	UnmapViewOfFile(file_view); // 프로세스의 주소 공간 내의 특정 영역에 매핑된 데이터 파일을 더 이상 유지하지 않음. 뷰를 해제.
	CloseHandle(file_map);
	CloseHandle(file_handle);

	return true;
}



int main(void)
{
	create_bob_txt();
	copy_bob();
}