// WinForm_DSHOW.cpp : main project file.

#include "stdafx.h"
#include "Form1.h"

using namespace camera_dip;

[STAThreadAttribute]
int main(array<System::String ^> ^args)
{
	// Enabling Windows XP visual effects before any controls are created
	Application::EnableVisualStyles();
	Application::SetCompatibleTextRenderingDefault(false); 

	// Create the main window and run it
	//Application::Run(gcnew Form1());
	Form1^ form1 = gcnew Form1();

	if(!form1->APP_FAILED)
		Application::Run(form1);
	
	//CoUninitialize();
	System::Windows::Forms::Application::Exit();
	return 0;
}
