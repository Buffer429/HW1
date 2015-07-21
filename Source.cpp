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
	// current directory �� ���Ѵ�.
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

	// current dir \\ test.txt ���ϸ� ����
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
	// 4Gb �̻��� ������ ��� MapViewOfFile()���� ������ ���ų�
	// ���� ������ �̵��� ������ ��
	// FilIoHelperClass ����� �̿��ؾ� ��
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
	//���� ����

	CopyFile(file_name, file_copy, 0);

	return TRUE;
}

bool create_bob_txt()
{
	// current directory �� ���Ѵ�.
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

	wchar_t file_name[260];           // bob1���� ������ ������ ���� �غ�.
	if (!SUCCEEDED(StringCbPrintfW(
		file_name,
		sizeof(file_name),
		L"%ws\\bob2.txt", buf)))   {
		print("err, can not create file name");
		free(buf);
		return false;
	}

	free(buf); buf = NULL;

	HANDLE file_handle = CreateFileW( // �޸𸮿� ������ ������ �ڵ��� ��´�. ���� �� INVALID_HANDLE_VALUE�� ��ȯ
		(LPCWSTR)file_name,
		GENERIC_READ,
		NULL,
		NULL,
		OPEN_EXISTING, //���� ����
		FILE_ATTRIBUTE_NORMAL,
		NULL
		);

	if (INVALID_HANDLE_VALUE == file_handle) {
		print("err, CreateFile(%ws) failed, gle = %u", file_name, GetLastError());
		return false;
	}

	LARGE_INTEGER fileSize;
	if (TRUE != GetFileSizeEx(file_handle, &fileSize)) { // ������ ������ ��ü ũ�⸦ ���Ѵ�.
		print("err, GetFileSizeEx(%ws) failed, gle = %u", file_name, GetLastError());
		CloseHandle(file_handle);
		return false;
	}
	_ASSERTE(fileSize.HighPart == 0); // ���� ����ϰ� ����� false �϶�, ���� �޽����� ����ϰ� ���α׷��� �ߴ�
	/*
	assert ��ũ�δ� �Ϲ������� ���α׷��� �ùٸ��� �۵����� ������ false �� ����ϱ� ���� expression �μ��� ���������ν�
	���α׷��� �����ϴ� ���� ������ �������� �ĺ��ϱ� ���� ���ȴ�.
	������� ������ ������, NDEBUG �ĺ��ڸ� �����Ͽ� �ҽ� ������ �������� �ʰ� ���� �˻縦 �� �� �ִ�.
	*/
	if (fileSize.HighPart > 0) {
		print("file size = %I64d (over 4GB) can not handle. use FileIoHelperClass",
			fileSize.QuadPart);
		CloseHandle(file_handle);
		return false;
	}

	DWORD file_size = (DWORD)fileSize.QuadPart;
	/*
	�ռ� CreateFile()�� ȣ���� ������ ���� ������ ������ ������ ���� ����Ҹ� �ü������ �˷��ֱ� ����.
	������ ������ ������� ���� ���� ������Ʈ�� �����Ѵ�.
	���� ������ ���� ���� ������Ʈ�� �����ϸ�, => reserve
	���� ���� ������Ʈ�� ���� ����� ���� ����Ұ� �����Ѵٴ� ���� Ȯ�ν�Ű�� �۾�.
	���н� NULL ��ȯ
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

	// UTF-8 -> UTF-16(UCS-2 ����)
	wchar_t *convchar5 = Utf8MbsToWcs(file_view);
	_wsetlocale(LC_ALL, L"korean");
	wprintf(L"%s\n", convchar5); // ��κ��� ���� �����͸� �ֿܼ� ���.

	UnmapViewOfFile(file_view); // ���μ����� �ּ� ���� ���� Ư�� ������ ���ε� ������ ������ �� �̻� �������� ����. �並 ����.
	CloseHandle(file_map);
	CloseHandle(file_handle);

	return true;
}



int main(void)
{
	create_bob_txt();
	copy_bob();
}