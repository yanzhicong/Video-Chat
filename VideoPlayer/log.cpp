#pragma once

#include "stdafx.h"
#include "log.h"

std::ofstream logfile;

void OpenLogFile()
{
	logfile.open("log.txt", std::ios::out | std::ios::trunc);
	if (!logfile)
	{
		AfxMessageBox(_T("Warning : Open Log File Failed !"));
	}
}

void Log(const char *str)
{
	if (logfile)
	{
		logfile << str;
	}
}

void Log(const int number)
{
	if (logfile)
	{
		logfile << number;
	}
}

void Log(const char *str, int number)
{
	if (logfile)
	{
		logfile << str << " : " << number << " \t";
	}
}

void Log(const char *str, signed long long number)
{
	if (logfile)
	{
		logfile << str << " : " << number << " \t";
	}
}

void Log(const char *str, int num, int den)
{
	if (logfile)
	{
		logfile << str << " : " << num << "/" << den << " \t";
	}
}


void Log()
{
	if (logfile)
	{
		logfile << std::endl;
	}
}

void CloseLogFile()
{
	logfile.close();
}