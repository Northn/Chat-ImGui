#pragma once

std::string cp1251_to_utf8(const char* str)
{
	std::string res;
	WCHAR* ures = NULL;
	char* cres = NULL;

	int result_u = MultiByteToWideChar(1251, 0, str, -1, 0, 0);
	if (result_u != 0)
	{
		ures = new WCHAR[result_u];
		if (MultiByteToWideChar(1251, 0, str, -1, ures, result_u))
		{
			int result_c = WideCharToMultiByte(CP_UTF8, 0, ures, -1, 0, 0, 0, 0);
			if (result_c != 0)
			{
				cres = new char[result_c];
				if (WideCharToMultiByte(CP_UTF8, 0, ures, -1, cres, result_c, 0, 0))
				{
					res = cres;
				}
			}
		}
	}

	delete[] ures;
	delete[] cres;

	return res;
}
