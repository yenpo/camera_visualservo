//#include "dshow_camera.h"
//#import "qedit.dll"
#include <windows.h>
#include <dshow.h>
#include <qedit.h>
#include <comdef.h>
#include <assert.h>
#include "SampleGrabberCallback.h"
#include "failid.h"

#define WM_GRAPHNOTIFY  WM_APP+1
#define CHECK_HR(hr) if (FAILED(hr)) { goto done; }
#define SAFE_RELEASE(x) if (x) { x->Release(); x = NULL; }
#define DEBUG
#define DEBUG_GRAPH_EDIT
#ifdef DEBUG_GRAPH_EDIT
	#define GRF_SAVE_DIR	L"camera_dip.GRF"
#endif

#define SAVE_SNAP		// To save the image on clicking "One shot" botton
#define MMTIMER			// Enable multimedia timer for tasking.
#define MAX_RESOL_LIST_NUM	500	// Max. number of item of the resolution list.
HWND ghApp=0;
DWORD g_dwGraphRegister=0;


// DirectShow Interface
IVideoWindow  * pVideoWindow = NULL;
IMediaControl * pMediaCtrl = NULL;
IMediaEventEx * pMediaEv = NULL;
IGraphBuilder * pGraph = NULL;
ICaptureGraphBuilder2 * pCapture = NULL;
IBaseFilter* pCaptureFilter = NULL;	// 
IBaseFilter *pGrabberF = NULL;
ISampleGrabber *pGrabber = NULL;
IBaseFilter *pNullF = NULL;
IBaseFilter *pSourceF = NULL;
IEnumPins *pEnum = NULL;
IPin *pPin = NULL;
IMoniker * pMonikerList[10];
IMoniker *pMoniker;
IReferenceClock *pClock = 0;
// Image buffer pointer.
BYTE *pBuffer = NULL;

LPOLESTR nameList[10];
UINT resolList[MAX_RESOL_LIST_NUM]={}; // Store the index of the device format.
UINT devNum;		// Total video device number.
UINT resolNum;
UINT currentDev;	// Index of current enabled video device
UINT oneShotCount = 0; // Index for naming the saved bmp file.
volatile HRESULT hr;

// Multimedia timer 
bool timerIsRunning = false;
MMRESULT mmResult;
MMRESULT mmTimerID;
UINT	mmtimerIntval;
UINT tickCount;
UINT tickCount2;
double dCount = 0.0;

// Grabber Callback
bool grabberCBIsRunning = false;

#ifdef DEBUG
        DWORD dwRegister;
#endif

#pragma once


namespace camera_dip {

	using namespace System;
	using namespace System::ComponentModel;
	using namespace System::Collections;
	using namespace System::Windows::Forms;
	using namespace System::Data;
	using namespace System::Drawing;	
	using namespace System::Drawing::Imaging;
	using namespace System::Diagnostics; //For stopwatch	
	using namespace SampleGrabber_CB;
	using namespace System::Runtime::InteropServices; // For delegate
	
	// For mmtimer
	delegate void TimerEventHandler( int id, int msg, IntPtr user, int dw1, int dw2 );
	delegate void TestEventHandler();
	[DllImport("winmm.dll")]
	extern "C" int timeSetEvent(int delay, int resolution, TimerEventHandler ^handler, IntPtr user, int eventType);
	[DllImport("winmm.dll")]
	extern "C" int timeKillEvent(int id);
	[DllImport("winmm.dll")]
	extern "C" int timeBeginPeriod(int msec);
	[DllImport("winmm.dll")]
	extern "C" int timeEndPeriod(int msec);
	/// <summary>
	/// Summary for Form1
	/// </summary>
	public ref class Form1 : public System::Windows::Forms::Form
	{
	public:
		Form1(void)
		{
			InitializeComponent();
			//
			//TODO: Add the constructor code here

			//Initialize COM Apartment-threading
			#ifdef DEBUG
			gstopWatch= gcnew Stopwatch();
			#endif

			gsampleGrabberCB = new SampleGrabberCallback();
			
			if(FAILED(CoInitializeEx(NULL, COINIT_APARTMENTTHREADED)))
			{
				System::Windows::Forms::MessageBox::Show("CoInitialize Failed!\r\n");  
				this->APP_FAILED = true;
				return;
			} 			
			hr = CreateInterface();
			if (FAILED(hr))
			{
				System::Windows::Forms::MessageBox::Show("CreateInterface() Failed\r\n");  
				this->APP_FAILED = true;
				return;
			}
			hr = FindDevices();
			if (FAILED(hr))
			{
				System::Windows::Forms::MessageBox::Show("FindDevices() Failed\r\n");  
				this->APP_FAILED = true;
				return;
			}
			 
			hr = BuildGraph(&pCaptureFilter);
			if (FAILED(hr))
			{
				System::Windows::Forms::MessageBox::Show("BuildGraph() Failed\r\n");  
				this->APP_FAILED = true;
				return;
			}
#ifdef DEBUG_GRAPH_EDIT
			hr = AddToRot(pGraph, &dwRegister);
			if (FAILED(hr))
				this->textBox1->AppendText("AddToRot Failed.\r\n");
			hr = SaveGraphFile(pGraph,GRF_SAVE_DIR);
			if (SUCCEEDED(hr))
				this->textBox1->AppendText("GRF file is saved in root Dir..");
			else
				this->textBox1->AppendText("Save GRF file failed.");
#endif							
			InitPictureBox();
		}

	protected:
		/// <summary>
		/// Clean up any resources being used.
		/// </summary>
		~Form1()
		{
			if (components)
			{
				delete components;
			}
		}
	//private: HRESULT FindDevices(void);
	private: System::Windows::Forms::MenuStrip^  menuStrip1;
	protected: 
	private: System::Windows::Forms::ToolStripMenuItem^  fileToolStripMenuItem;
	private: System::Windows::Forms::ToolStripMenuItem^  optionToolStripMenuItem;
	private: System::Windows::Forms::ToolStripMenuItem^  aboutToolStripMenuItem;
	private: System::Windows::Forms::ToolStripMenuItem^  aboutToolStripMenuItem1;
	private: System::Windows::Forms::PictureBox^  pictureBox1;
	private: System::Windows::Forms::StatusStrip^  statusStrip1;
	private: System::Windows::Forms::TextBox^  textBox1;
	private: System::Windows::Forms::ToolStripMenuItem^  exitToolStripMenuItem;
	private: System::Windows::Forms::ToolStripMenuItem^  devicesToolStripMenuItem;
	private: System::Windows::Forms::ToolStripMenuItem^  devicesItem;
	private: System::Windows::Forms::ToolStripMenuItem^  testToolStripMenuItem;
	private: System::Windows::Forms::ToolStripMenuItem^  tEST1ToolStripMenuItem;
	private: System::Windows::Forms::ToolStripMenuItem^  tEST2ToolStripMenuItem;
	private: System::Windows::Forms::ToolStripStatusLabel^  toolStripStatusLabel1;
	private: System::Windows::Forms::ToolStripStatusLabel^  toolStripStatusLabel2;
	private: System::Windows::Forms::ToolStripStatusLabel^  toolStripStatusLabel3;
	private: System::Windows::Forms::ToolStripMenuItem^  resolutionToolStripMenuItem;
	private: System::Windows::Forms::ToolStripMenuItem^  resolutionItem;
	
	// For rendering the pictureBox1
	private: Bitmap^ dispBMP;
	private: Bitmap^ sampleBMP;
	private: BitmapData^ dispBMPData;
	private: BitmapData^ sampleBMPData;
	private: System::Drawing::Rectangle rect;

	private: String^	bmpFormat;
	public:	 bool	APP_FAILED;	
	
	// MMTimer		
	private: TimerEventHandler^ timerEventHandler;	
	private: System::Windows::Forms::Timer^ timer1;
	SampleGrabberCallback* gsampleGrabberCB;


	#ifdef DEBUG
	Stopwatch^ gstopWatch;	
	#endif
private: System::Windows::Forms::Button^  button1;
private: System::Windows::Forms::Button^  button2;
private: System::Windows::Forms::TextBox^  textBox2;
private: System::Windows::Forms::Button^  button3;

private: System::Windows::Forms::Label^  label1;
private: System::Windows::Forms::Label^  label2;
private: System::Windows::Forms::Label^  label3;
private: System::Windows::Forms::Label^  label4;
private: System::Windows::Forms::ToolStripStatusLabel^  toolStripStatusLabel4;
private: System::Windows::Forms::ToolStripMenuItem^  capturePinToolStripMenuItem;
private: System::Windows::Forms::ToolStripMenuItem^  cameraPropertyToolStripMenuItem;

	private:
		/// <summary>
		/// Required designer variable.
		/// </summary>
		System::ComponentModel::Container ^components;

#pragma region Windows Form Designer generated code
		/// <summary>
		/// Required method for Designer support - do not modify
		/// the contents of this method with the code editor.
		/// </summary>
		void InitializeComponent(void)
		{
			this->menuStrip1 = (gcnew System::Windows::Forms::MenuStrip());
			this->fileToolStripMenuItem = (gcnew System::Windows::Forms::ToolStripMenuItem());
			this->exitToolStripMenuItem = (gcnew System::Windows::Forms::ToolStripMenuItem());
			this->devicesToolStripMenuItem = (gcnew System::Windows::Forms::ToolStripMenuItem());
			this->optionToolStripMenuItem = (gcnew System::Windows::Forms::ToolStripMenuItem());
			this->resolutionToolStripMenuItem = (gcnew System::Windows::Forms::ToolStripMenuItem());
			this->capturePinToolStripMenuItem = (gcnew System::Windows::Forms::ToolStripMenuItem());
			this->cameraPropertyToolStripMenuItem = (gcnew System::Windows::Forms::ToolStripMenuItem());
			this->aboutToolStripMenuItem = (gcnew System::Windows::Forms::ToolStripMenuItem());
			this->aboutToolStripMenuItem1 = (gcnew System::Windows::Forms::ToolStripMenuItem());
			this->testToolStripMenuItem = (gcnew System::Windows::Forms::ToolStripMenuItem());
			this->tEST1ToolStripMenuItem = (gcnew System::Windows::Forms::ToolStripMenuItem());
			this->tEST2ToolStripMenuItem = (gcnew System::Windows::Forms::ToolStripMenuItem());
			this->pictureBox1 = (gcnew System::Windows::Forms::PictureBox());
			this->statusStrip1 = (gcnew System::Windows::Forms::StatusStrip());
			this->toolStripStatusLabel1 = (gcnew System::Windows::Forms::ToolStripStatusLabel());
			this->toolStripStatusLabel2 = (gcnew System::Windows::Forms::ToolStripStatusLabel());
			this->toolStripStatusLabel3 = (gcnew System::Windows::Forms::ToolStripStatusLabel());
			this->toolStripStatusLabel4 = (gcnew System::Windows::Forms::ToolStripStatusLabel());
			this->textBox1 = (gcnew System::Windows::Forms::TextBox());
			this->button1 = (gcnew System::Windows::Forms::Button());
			this->button2 = (gcnew System::Windows::Forms::Button());
			this->textBox2 = (gcnew System::Windows::Forms::TextBox());
			this->button3 = (gcnew System::Windows::Forms::Button());
			this->label1 = (gcnew System::Windows::Forms::Label());
			this->label2 = (gcnew System::Windows::Forms::Label());
			this->label3 = (gcnew System::Windows::Forms::Label());
			this->label4 = (gcnew System::Windows::Forms::Label());
			this->menuStrip1->SuspendLayout();
			(cli::safe_cast<System::ComponentModel::ISupportInitialize^  >(this->pictureBox1))->BeginInit();
			this->statusStrip1->SuspendLayout();
			this->SuspendLayout();
			// 
			// menuStrip1
			// 
			this->menuStrip1->Items->AddRange(gcnew cli::array< System::Windows::Forms::ToolStripItem^  >(5) {this->fileToolStripMenuItem, 
				this->devicesToolStripMenuItem, this->optionToolStripMenuItem, this->aboutToolStripMenuItem, this->testToolStripMenuItem});
			this->menuStrip1->Location = System::Drawing::Point(0, 0);
			this->menuStrip1->Name = L"menuStrip1";
			this->menuStrip1->Size = System::Drawing::Size(1064, 24);
			this->menuStrip1->TabIndex = 0;
			this->menuStrip1->Text = L"menuStrip1";
			// 
			// fileToolStripMenuItem
			// 
			this->fileToolStripMenuItem->DropDownItems->AddRange(gcnew cli::array< System::Windows::Forms::ToolStripItem^  >(1) {this->exitToolStripMenuItem});
			this->fileToolStripMenuItem->Name = L"fileToolStripMenuItem";
			this->fileToolStripMenuItem->Size = System::Drawing::Size(39, 20);
			this->fileToolStripMenuItem->Text = L"File";
			// 
			// exitToolStripMenuItem
			// 
			this->exitToolStripMenuItem->Name = L"exitToolStripMenuItem";
			this->exitToolStripMenuItem->Size = System::Drawing::Size(96, 22);
			this->exitToolStripMenuItem->Text = L"Exit";
			this->exitToolStripMenuItem->Click += gcnew System::EventHandler(this, &Form1::exitToolStripMenuItem_Click);
			// 
			// devicesToolStripMenuItem
			// 
			this->devicesToolStripMenuItem->Name = L"devicesToolStripMenuItem";
			this->devicesToolStripMenuItem->Size = System::Drawing::Size(63, 20);
			this->devicesToolStripMenuItem->Text = L"Devices";
			// 
			// optionToolStripMenuItem
			// 
			this->optionToolStripMenuItem->DropDownItems->AddRange(gcnew cli::array< System::Windows::Forms::ToolStripItem^  >(3) {this->resolutionToolStripMenuItem, 
				this->capturePinToolStripMenuItem, this->cameraPropertyToolStripMenuItem});
			this->optionToolStripMenuItem->Name = L"optionToolStripMenuItem";
			this->optionToolStripMenuItem->Size = System::Drawing::Size(60, 20);
			this->optionToolStripMenuItem->Text = L"Option";
			// 
			// resolutionToolStripMenuItem
			// 
			this->resolutionToolStripMenuItem->Name = L"resolutionToolStripMenuItem";
			this->resolutionToolStripMenuItem->Size = System::Drawing::Size(171, 22);
			this->resolutionToolStripMenuItem->Text = L"Resolution";
			// 
			// capturePinToolStripMenuItem
			// 
			this->capturePinToolStripMenuItem->Name = L"capturePinToolStripMenuItem";
			this->capturePinToolStripMenuItem->Size = System::Drawing::Size(171, 22);
			this->capturePinToolStripMenuItem->Text = L"Capture Pin";
			this->capturePinToolStripMenuItem->Click += gcnew System::EventHandler(this, &Form1::capturePinToolStripMenuItem_Click);
			// 
			// cameraPropertyToolStripMenuItem
			// 
			this->cameraPropertyToolStripMenuItem->Name = L"cameraPropertyToolStripMenuItem";
			this->cameraPropertyToolStripMenuItem->Size = System::Drawing::Size(171, 22);
			this->cameraPropertyToolStripMenuItem->Text = L"Camera Property";
			this->cameraPropertyToolStripMenuItem->Click += gcnew System::EventHandler(this, &Form1::cameraPropertyToolStripMenuItem_Click);
			// 
			// aboutToolStripMenuItem
			// 
			this->aboutToolStripMenuItem->DropDownItems->AddRange(gcnew cli::array< System::Windows::Forms::ToolStripItem^  >(1) {this->aboutToolStripMenuItem1});
			this->aboutToolStripMenuItem->Name = L"aboutToolStripMenuItem";
			this->aboutToolStripMenuItem->Size = System::Drawing::Size(47, 20);
			this->aboutToolStripMenuItem->Text = L"Help";
			// 
			// aboutToolStripMenuItem1
			// 
			this->aboutToolStripMenuItem1->Name = L"aboutToolStripMenuItem1";
			this->aboutToolStripMenuItem1->Size = System::Drawing::Size(111, 22);
			this->aboutToolStripMenuItem1->Text = L"About";
			this->aboutToolStripMenuItem1->Click += gcnew System::EventHandler(this, &Form1::aboutToolStripMenuItem1_Click);
			// 
			// testToolStripMenuItem
			// 
			this->testToolStripMenuItem->DropDownItems->AddRange(gcnew cli::array< System::Windows::Forms::ToolStripItem^  >(2) {this->tEST1ToolStripMenuItem, 
				this->tEST2ToolStripMenuItem});
			this->testToolStripMenuItem->Name = L"testToolStripMenuItem";
			this->testToolStripMenuItem->Size = System::Drawing::Size(40, 20);
			this->testToolStripMenuItem->Text = L"test";
			// 
			// tEST1ToolStripMenuItem
			// 
			this->tEST1ToolStripMenuItem->CheckOnClick = true;
			this->tEST1ToolStripMenuItem->Name = L"tEST1ToolStripMenuItem";
			this->tEST1ToolStripMenuItem->Size = System::Drawing::Size(111, 22);
			this->tEST1ToolStripMenuItem->Text = L"TEST1";
			this->tEST1ToolStripMenuItem->Click += gcnew System::EventHandler(this, &Form1::tEST1ToolStripMenuItem_Click);
			// 
			// tEST2ToolStripMenuItem
			// 
			this->tEST2ToolStripMenuItem->Name = L"tEST2ToolStripMenuItem";
			this->tEST2ToolStripMenuItem->Size = System::Drawing::Size(111, 22);
			this->tEST2ToolStripMenuItem->Text = L"TEST2";
			// 
			// pictureBox1
			// 
			this->pictureBox1->Location = System::Drawing::Point(13, 28);
			this->pictureBox1->Name = L"pictureBox1";
			this->pictureBox1->Size = System::Drawing::Size(640, 480);
			this->pictureBox1->SizeMode = System::Windows::Forms::PictureBoxSizeMode::StretchImage;
			this->pictureBox1->TabIndex = 1;
			this->pictureBox1->TabStop = false;
			// 
			// statusStrip1
			// 
			this->statusStrip1->Items->AddRange(gcnew cli::array< System::Windows::Forms::ToolStripItem^  >(4) {this->toolStripStatusLabel1, 
				this->toolStripStatusLabel2, this->toolStripStatusLabel3, this->toolStripStatusLabel4});
			this->statusStrip1->Location = System::Drawing::Point(0, 516);
			this->statusStrip1->Name = L"statusStrip1";
			this->statusStrip1->Size = System::Drawing::Size(1064, 29);
			this->statusStrip1->TabIndex = 2;
			this->statusStrip1->Text = L"statusStrip1";
			// 
			// toolStripStatusLabel1
			// 
			this->toolStripStatusLabel1->AutoSize = false;
			this->toolStripStatusLabel1->Name = L"toolStripStatusLabel1";
			this->toolStripStatusLabel1->Size = System::Drawing::Size(115, 24);
			this->toolStripStatusLabel1->Text = L"Device";
			this->toolStripStatusLabel1->TextAlign = System::Drawing::ContentAlignment::MiddleLeft;
			// 
			// toolStripStatusLabel2
			// 
			this->toolStripStatusLabel2->AutoSize = false;
			this->toolStripStatusLabel2->Name = L"toolStripStatusLabel2";
			this->toolStripStatusLabel2->Size = System::Drawing::Size(60, 24);
			this->toolStripStatusLabel2->Text = L"Resol.";
			this->toolStripStatusLabel2->TextAlign = System::Drawing::ContentAlignment::MiddleLeft;
			// 
			// toolStripStatusLabel3
			// 
			this->toolStripStatusLabel3->AutoSize = false;
			this->toolStripStatusLabel3->Name = L"toolStripStatusLabel3";
			this->toolStripStatusLabel3->Size = System::Drawing::Size(60, 24);
			this->toolStripStatusLabel3->Text = L"# of BITS";
			this->toolStripStatusLabel3->TextAlign = System::Drawing::ContentAlignment::MiddleLeft;
			// 
			// toolStripStatusLabel4
			// 
			this->toolStripStatusLabel4->Name = L"toolStripStatusLabel4";
			this->toolStripStatusLabel4->Size = System::Drawing::Size(48, 24);
			this->toolStripStatusLabel4->Text = L"@#FPS";
			// 
			// textBox1
			// 
			this->textBox1->Location = System::Drawing::Point(663, 290);
			this->textBox1->Multiline = true;
			this->textBox1->Name = L"textBox1";
			this->textBox1->ScrollBars = System::Windows::Forms::ScrollBars::Vertical;
			this->textBox1->Size = System::Drawing::Size(389, 223);
			this->textBox1->TabIndex = 3;
			// 
			// button1
			// 
			this->button1->Location = System::Drawing::Point(741, 57);
			this->button1->Name = L"button1";
			this->button1->Size = System::Drawing::Size(75, 23);
			this->button1->TabIndex = 4;
			this->button1->Text = L"One Shot";
			this->button1->UseVisualStyleBackColor = true;
			this->button1->Click += gcnew System::EventHandler(this, &Form1::button1_Click);
			// 
			// button2
			// 
			this->button2->Location = System::Drawing::Point(660, 57);
			this->button2->Name = L"button2";
			this->button2->Size = System::Drawing::Size(75, 23);
			this->button2->TabIndex = 5;
			this->button2->Text = L"Run";
			this->button2->UseVisualStyleBackColor = true;
			this->button2->Click += gcnew System::EventHandler(this, &Form1::button2_Click);
			// 
			// textBox2
			// 
			this->textBox2->Location = System::Drawing::Point(659, 112);
			this->textBox2->Name = L"textBox2";
			this->textBox2->Size = System::Drawing::Size(74, 22);
			this->textBox2->TabIndex = 6;
			this->textBox2->Text = L"100";
			// 
			// button3
			// 
			this->button3->Location = System::Drawing::Point(659, 28);
			this->button3->Name = L"button3";
			this->button3->Size = System::Drawing::Size(157, 23);
			this->button3->TabIndex = 7;
			this->button3->Text = L"Enable Camera";
			this->button3->UseVisualStyleBackColor = true;
			this->button3->Click += gcnew System::EventHandler(this, &Form1::button3_Click);
			// 
			// label1
			// 
			this->label1->AutoSize = true;
			this->label1->Font = (gcnew System::Drawing::Font(L"PMingLiU", 10));
			this->label1->Location = System::Drawing::Point(660, 267);
			this->label1->Name = L"label1";
			this->label1->Size = System::Drawing::Size(54, 14);
			this->label1->TabIndex = 9;
			this->label1->Text = L"Message";
			// 
			// label2
			// 
			this->label2->AutoSize = true;
			this->label2->Font = (gcnew System::Drawing::Font(L"PMingLiU", 10));
			this->label2->Location = System::Drawing::Point(928, 33);
			this->label2->Name = L"label2";
			this->label2->Size = System::Drawing::Size(32, 14);
			this->label2->TabIndex = 10;
			this->label2->Text = L"FPS:";
			// 
			// label3
			// 
			this->label3->AutoSize = true;
			this->label3->Font = (gcnew System::Drawing::Font(L"PMingLiU", 9.75F, System::Drawing::FontStyle::Regular, System::Drawing::GraphicsUnit::Point, 
				static_cast<System::Byte>(136)));
			this->label3->Location = System::Drawing::Point(656, 96);
			this->label3->Name = L"label3";
			this->label3->Size = System::Drawing::Size(90, 13);
			this->label3->TabIndex = 11;
			this->label3->Text = L"Interrupt Interval:";
			// 
			// label4
			// 
			this->label4->AutoSize = true;
			this->label4->Font = (gcnew System::Drawing::Font(L"PMingLiU", 12));
			this->label4->Location = System::Drawing::Point(966, 31);
			this->label4->Name = L"label4";
			this->label4->Size = System::Drawing::Size(16, 16);
			this->label4->TabIndex = 12;
			this->label4->Text = L"0";
			// 
			// Form1
			// 
			this->AutoScaleDimensions = System::Drawing::SizeF(6, 12);
			this->AutoScaleMode = System::Windows::Forms::AutoScaleMode::Font;
			this->ClientSize = System::Drawing::Size(1064, 545);
			this->Controls->Add(this->label4);
			this->Controls->Add(this->label3);
			this->Controls->Add(this->label2);
			this->Controls->Add(this->label1);
			this->Controls->Add(this->button3);
			this->Controls->Add(this->textBox2);
			this->Controls->Add(this->button2);
			this->Controls->Add(this->button1);
			this->Controls->Add(this->textBox1);
			this->Controls->Add(this->statusStrip1);
			this->Controls->Add(this->pictureBox1);
			this->Controls->Add(this->menuStrip1);
			this->MainMenuStrip = this->menuStrip1;
			this->Name = L"Form1";
			this->Text = L"Form1";
			this->FormClosing += gcnew System::Windows::Forms::FormClosingEventHandler(this, &Form1::Form1_FormClosing);
			this->menuStrip1->ResumeLayout(false);
			this->menuStrip1->PerformLayout();
			(cli::safe_cast<System::ComponentModel::ISupportInitialize^  >(this->pictureBox1))->EndInit();
			this->statusStrip1->ResumeLayout(false);
			this->statusStrip1->PerformLayout();
			this->ResumeLayout(false);
			this->PerformLayout();

		}
#pragma endregion


private: System::Void Form1_FormClosing(System::Object^  sender, System::Windows::Forms::FormClosingEventArgs^  e) {

#ifdef DEBUG
			 RemoveFromRot(dwRegister);
			 delete gstopWatch;
#endif
			 delete gsampleGrabberCB;
			 pMediaCtrl->Stop();
			 System::Windows::Forms::MessageBox::Show("Bye!");
		 }
			
private: System::Void exitToolStripMenuItem_Click(System::Object^  sender, System::EventArgs^  e) {
			 Application::Exit();
		 }
private: System::Void aboutToolStripMenuItem1_Click(System::Object^  sender, System::EventArgs^  e) {
			 System::Windows::Forms::MessageBox::Show("Developer: YP Wang");
		 }
private: System::Void devicesItem_Click(System::Object^  sender, System::EventArgs^  e)
		 {			 
			 ToolStripMenuItem^ itemSender = (ToolStripMenuItem^) sender;
			 int senderNumber = 0;
			 // Retrieve devices list
			 array<ToolStripMenuItem^>^ itemList = gcnew array<ToolStripMenuItem^>(devNum);
			 for (UINT i=0;i<devNum;i++)
			 {
				 itemList[i] = (ToolStripMenuItem^)this->devicesToolStripMenuItem->DropDownItems[i];
				 if (itemSender->Text == itemList[i]->Text)
				 {
					 senderNumber = i;
				 }
				 if (itemList[i]->CheckState == CheckState::Checked)
				 {
					 if (itemSender->Text == itemList[i]->Text)
					 {
						 return;
					 }
				 }
				 itemList[i]->CheckState = CheckState::Unchecked;					
			 }
			 ChooseDevice(senderNumber,&pCaptureFilter);
			 currentDev = senderNumber;
			 BuildGraph(&pCaptureFilter);
			 InitSampleGrabber(TRUE);
			 delete itemList;

			#ifdef DEBUG
			 if(itemSender->CheckState==CheckState::Checked)
			 {
				this->textBox1->AppendText(itemSender->Text);
				this->textBox1->AppendText(" is enabled.");
				this->textBox1->AppendText("\n");
			 }
			#endif
			 //System::Windows::Forms::MessageBox::Show("Bye!");
		 }
private: System::Void resolutionItem_Click(System::Object^  sender, System::EventArgs^  e)
		 {
			 ToolStripMenuItem^ itemSender = (ToolStripMenuItem^) sender;
			 int senderNumber = 0;
			 // Retrieve devices list
			 array<ToolStripMenuItem^>^ itemList = gcnew array<ToolStripMenuItem^>(resolNum);
			 for (UINT i=0;i<resolNum;i++)
			 {
				 itemList[i] = (ToolStripMenuItem^)this->resolutionToolStripMenuItem->DropDownItems[i];
				 if (itemSender->Text == itemList[i]->Text)
				 {
					 pMediaCtrl->Stop();
					 hr = DisconnectFilters(pGraph, pGrabberF, pNullF);	
					 hr = DisconnectFilters(pGraph, pCaptureFilter, pGrabberF);
					 hr = SetResolution(&pCaptureFilter,resolList[i]);
					 if(FAILED(hr))
					 {
						 System::Windows::Forms::MessageBox::Show("Set resolution failed.\r\n");
						 return;
					 }
					 hr = BuildGraph(&pCaptureFilter);
					 if (FAILED(hr))
					 {
							System::Windows::Forms::MessageBox::Show("BuildGraph() Failed\r\n");  							
							return;
					 }
					 break;
				 }				 				
			 }			 
		 }
private: HRESULT CreateInterface(){
			 /*----------------------------------------------------------------*\
				Get interface
			\*----------------------------------------------------------------*/
		    // Create the filter graph
			hr = CoCreateInstance (CLSID_FilterGraph, NULL, CLSCTX_INPROC,
								   IID_IGraphBuilder, (void **) &pGraph);
			if (FAILED(hr))
				ShowFailedMessage(CREATE_GRAPH_FAILED);
			// Create the capture graph builder
			hr = CoCreateInstance (CLSID_CaptureGraphBuilder2 , NULL, CLSCTX_INPROC,
                             IID_ICaptureGraphBuilder2, (void **) &pCapture);
			if (FAILED(hr))
				ShowFailedMessage(CREATE_CAPTURE_FAILED);		

			// Obtain interfaces for media control and Video Window and media event and Sample grabber	
			hr = pGraph->QueryInterface(IID_IMediaControl,(LPVOID *) &pMediaCtrl);
			if (FAILED(hr))
				ShowFailedMessage(OBTAIN_IF_MC_FAILED);
			
			hr = pGraph->QueryInterface(IID_IVideoWindow, (LPVOID *) &pVideoWindow);
			if (FAILED(hr))
				ShowFailedMessage(OBTAIN_IF_VW_FAILED);

			hr = pGraph->QueryInterface(IID_IMediaEventEx, (LPVOID *) &pMediaEv);
			if (FAILED(hr))
				ShowFailedMessage(OBTAIN_IF_ME_FAILED);

			// Set the window handle used to process graph events
	/*		hr = pMediaEv->SetNotifyWindow((OAHWND)ghApp, WM_GRAPHNOTIFY, 0);
			if (FAILED(hr))
				ShowFailedMessage(SET_WIN_HANDL_FAILED);*/

			// Sample grabber
			hr = CoCreateInstance(CLSID_SampleGrabber, NULL, CLSCTX_INPROC_SERVER,
								 IID_PPV_ARGS(&pGrabberF));
			if (FAILED(hr))
				ShowFailedMessage(CREATE_GRABBER_FAILED);

			 hr = pGraph->AddFilter(pGrabberF, L"Sample Grabber");
			if (FAILED(hr))
				ShowFailedMessage(ADD_FILTER_GRABBER_FAILED);	

			 hr = pGrabberF->QueryInterface(IID_PPV_ARGS(&pGrabber));
			 if (FAILED(hr))
				 ShowFailedMessage(OBTAIN_IF_GRABBER_FAILED);
			 
			 // Set the media type for the sample grabber.
			 AM_MEDIA_TYPE mt;
			 ZeroMemory(&mt, sizeof(mt));
			 mt.majortype = MEDIATYPE_Video;
			 mt.subtype = MEDIASUBTYPE_RGB24;
			 hr = pGrabber->SetMediaType(&mt);
			 if (FAILED(hr))
				 ShowFailedMessage(SET_MT_FAILED);

			// Create a null filter
			hr = CoCreateInstance(CLSID_NullRenderer, NULL, CLSCTX_INPROC_SERVER,IID_PPV_ARGS(&pNullF));
			if (FAILED(hr))
				ShowFailedMessage(CREATE_NULL_FILTER_FAILED);

			hr = pGraph->AddFilter(pNullF, L"Null Filter");
			if (FAILED(hr))
				ShowFailedMessage(ADD_NULL_FILTER_FAILED);

			// Attach the filter graph to the capture graph
			hr = pCapture->SetFiltergraph(pGraph);
			if (FAILED(hr))
				ShowFailedMessage(CAPTURE_INIT_FAILED);

			return hr;
		 }
private: HRESULT FindDevices(){
		HRESULT hr;
		ICreateDevEnum * pDevEnum;
		IEnumMoniker * pEnumMoniker;

		hr = CoCreateInstance (CLSID_SystemDeviceEnum, NULL, CLSCTX_INPROC,
							   IID_ICreateDevEnum, (void **) &pDevEnum);
		if(hr != S_OK)
		{			
			System::Windows::Forms::MessageBox::Show("No Video Device Found.\r\n");
			ShowComError(hr);
			return -1;
		}

		// Obtain a class enumerator for the video compressor category.		
		hr = pDevEnum->CreateClassEnumerator (CLSID_VideoInputDeviceCategory, &pEnumMoniker, 0);
		//hr = pDevEnum->CreateClassEnumerator (AM_KSCATEGORY_CAPTURE, &pEnumMoniker, 0);
		if (hr != S_OK)
		{
			System::Windows::Forms::MessageBox::Show("Enumerate device failed.\r\n");
			ShowComError(hr);
			return -1;
		}
		else if(hr == S_OK)   // Enumerate the device in the category.
		{			
			ULONG cFetched;
			while(hr = pEnumMoniker->Next(1, &pMoniker, &cFetched), hr==S_OK)
			{
				static int monikerCount=0;
				IPropertyBag *pPropBag;
				hr = pMoniker->BindToStorage(0, 0, IID_IPropertyBag,(void **)&pPropBag);
				if (FAILED(hr))
				{
					pMoniker->Release();
					continue; // Next may be available.
				}
				if (SUCCEEDED(hr))
				{
					// To retrieve the filter's friendly name, do the following:
					VARIANT varName;
					VariantInit(&varName);
					// The name of the device.
					hr = pPropBag->Read(L"FriendlyName", &varName, 0);
					// Display the name in your UI somehow.
					if (hr == S_OK)
					{								
						pMoniker->GetDisplayName(NULL,NULL,&nameList[monikerCount]);						
						pMonikerList[monikerCount]=pMoniker;
						pMoniker->AddRef();
						#ifdef DEBUG						
						this->textBox1->AppendText("Device names #");
						this->textBox1->AppendText(monikerCount.ToString());
						this->textBox1->AppendText("\r\n");
						this->textBox1->AppendText(gcnew System::String(nameList[monikerCount]));
						this->textBox1->AppendText("\r\n\r\n");
						#endif
						monikerCount++;						
						// Add the name to the "Devices" menu.
						this->devicesItem = gcnew ToolStripMenuItem();
						this->devicesToolStripMenuItem->DropDownItems->AddRange(gcnew cli::array< System::Windows::Forms::ToolStripItem^  >(1) {this->devicesItem});
						this->devicesItem->Text = gcnew System::String(varName.bstrVal);
						this->devicesItem->Click += gcnew System::EventHandler(this,&Form1::devicesItem_Click);
						this->devicesItem->CheckOnClick = false;
					}
					else
					{
						System::Windows::Forms::MessageBox::Show("Get friendly name failed.\r\n");
						ShowComError(hr);
						//hr = pPropBag->Read(L"DevicePath", &varName, 0);	
						return -1;
					}
					VariantClear(&varName);				
				}
				pPropBag->Release();
				devNum = monikerCount;
			}
			//Choose the first device as default camera after enumerating finished.
			currentDev = 0;
			hr = ChooseDevice(0,&pCaptureFilter);
			if(FAILED(hr))
			{
				System::Windows::Forms::MessageBox::Show("ChooseDevice() failed.\r\n");
			}
		}
		pEnumMoniker->Release();		
		return hr;
	}
private: HRESULT CreateKernelFilter(
    const GUID &guidCategory,  // Filter category.
    LPCOLESTR szName,          // The name of the filter.
    IBaseFilter **ppFilter     // Receives a pointer to the filter.
)
{
    HRESULT hr;
    ICreateDevEnum *pDevEnum = NULL;
    IEnumMoniker *pEnum = NULL;
    if (!szName || !ppFilter) 
    {
        return E_POINTER;
    }

    // Create the system device enumerator.
    hr = CoCreateInstance(CLSID_SystemDeviceEnum, NULL, CLSCTX_INPROC,
        IID_ICreateDevEnum, (void**)&pDevEnum);
    if (FAILED(hr))
    {
        return hr;
    }

    // Create a class enumerator for the specified category.
    hr = pDevEnum->CreateClassEnumerator(guidCategory, &pEnum, 0);
    pDevEnum->Release();
    if (hr != S_OK) // S_FALSE means the category is empty.
    {
        return E_FAIL;
    }

    // Enumerate devices within this category.
    bool bFound = false;
    IMoniker *pMoniker;
    while (!bFound && (S_OK == pEnum->Next(1, &pMoniker, 0)))
    {
        IPropertyBag *pBag = NULL;
        hr = pMoniker->BindToStorage(0, 0, IID_IPropertyBag, (void **)&pBag);
        if (FAILED(hr))
        {
            pMoniker->Release();
            continue; // Maybe the next one will work.
        }
        // Check the friendly name.
        VARIANT var;
        VariantInit(&var);
        hr = pBag->Read(L"FriendlyName", &var, NULL);
        if (SUCCEEDED(hr) && (lstrcmpiW(var.bstrVal, szName) == 0))
        {
            // This is the right filter.
            hr = pMoniker->BindToObject(0, 0, IID_IBaseFilter,
                (void**)ppFilter);
            bFound = true;
        }
        VariantClear(&var);
        pBag->Release();
        pMoniker->Release();
    }
    pEnum->Release();
    return (bFound ? hr : E_FAIL);
}
private: HRESULT BuildGraph(IBaseFilter **ppCaptureFilter){
			 
			//pGraph->AddFilter(pCaptureFilter,L"WDM Video Capture Filter");
			pGraph->AddFilter(pCaptureFilter,L"Video Capture Filter");

			hr = ConnectFilters(pGraph, pCaptureFilter, pGrabberF);
			if (FAILED(hr))
			{
				System::Windows::Forms::MessageBox::Show("Connect filters failed.\r\nCapture and Grabber");
				ShowComError(hr);
				return hr;
			}
			hr = ConnectFilters(pGraph, pGrabberF, pNullF);	
			if (FAILED(hr))
			{
				System::Windows::Forms::MessageBox::Show("Connect filters failed.\r\nGrabber and Null");
				ShowComError(hr);
				return hr;
			}
			//pCaptureFilter->Release();
			//hr = pCapture->RenderStream(&PIN_CATEGORY_PREVIEW, &MEDIATYPE_Video,pCaptureFilter, NULL, NULL);
			//if (hr == VFW_S_NOPREVIEWPIN)
				//this->textBox1->AppendText("Preview was rendered through the Smart Tee Filter.");
			return hr;
		 }
private: HRESULT ChooseDevice(int num, IBaseFilter **ppCaptureFilter){
			
			// Remove the Capture filter if it exists.
			if (*ppCaptureFilter != NULL)
			{
				pMediaCtrl->Stop();
				hr = DisconnectFilters(pGraph, pGrabberF, pNullF);
				if (FAILED(hr))
				{
					System::Windows::Forms::MessageBox::Show("Disconnect filters failed.\r\n");
					ShowComError(hr);
					(*ppCaptureFilter)->Release();
					return -1;
				}
				hr = DisconnectFilters(pGraph, pCaptureFilter, pGrabberF);				
				if (FAILED(hr))
				{
					System::Windows::Forms::MessageBox::Show("Disconnect filters failed.\r\n");
					ShowComError(hr);
					(*ppCaptureFilter)->Release();
					return -1;
				}
				hr = pGraph->RemoveFilter(*ppCaptureFilter);
				if (FAILED(hr))
				{
					System::Windows::Forms::MessageBox::Show("Remove filter failed.\r\n");
					ShowComError(hr);
					(*ppCaptureFilter)->Release();
					return -1;
				}					
				(*ppCaptureFilter)->Release();
			}
			for (UINT i=0;i<MAX_RESOL_LIST_NUM;i++)
			{
				resolList[i]=0;
			}
			IBindCtx * pBindCtx;
			hr = CreateBindCtx(0, &pBindCtx);
			if (hr == S_OK)
			{
				ULONG pchEaten;
				LPOLESTR monikerName;
				hr = MkParseDisplayName(pBindCtx,nameList[num],&pchEaten,&pMoniker);
				pMoniker->GetDisplayName(NULL,NULL,&monikerName);
				pBindCtx->Release();
				//Add the device friendly name to status label1
				ToolStripMenuItem^ item0 = (ToolStripMenuItem^)this->devicesToolStripMenuItem->DropDownItems[num];
				this->toolStripStatusLabel1->Text=item0->Text;
				item0->CheckState = CheckState::Checked;
				#ifdef DEBUG
				this->textBox1->AppendText("Device #");
				this->textBox1->AppendText(num.ToString());
				this->textBox1->AppendText(" is enabled:\r\n");
				this->textBox1->AppendText(gcnew System::String(monikerName));				
				this->textBox1->AppendText("\r\n");
				this->textBox1->AppendText("\r\n");
				#endif

				//// To create an instance of the filter, do the following:
				IBaseFilter *pFilter;
				hr = pMoniker->BindToObject(NULL, NULL, IID_IBaseFilter,(void**)&pFilter);
				
				if (FAILED(hr))
				{
					System::Windows::Forms::MessageBox::Show("BindToObject() failed.\r\n");
					ShowComError(hr);
					return hr;
				}		

				hr = GetDeviceFormat(&pFilter);
				if(FAILED(hr))
				{
					System::Windows::Forms::MessageBox::Show("GetDeviceFormat() failed.\r\n");
				}
				*ppCaptureFilter = pFilter;
				(*ppCaptureFilter)->AddRef();
				pFilter->Release();
				pMoniker->Release();
			}
			else
			{
				System::Windows::Forms::MessageBox::Show("CreateBindCtx() failed.\r\n");
				ShowComError(hr);
			}
			return hr;
		 }
private: int GetDeviceFormat(IBaseFilter **ppFilter){
				UINT resolCount = 0;
			 	IAMStreamConfig *pvideoConfig;
				AM_MEDIA_TYPE *pMt;
				VIDEOINFOHEADER* videoInfoHeader = NULL;
				//VIDEOINFOHEADER2* videoInfoHeader2 = NULL;
				BYTE *pSCC = NULL;
				int iCount = NULL;
				int iSize = NULL;

				if (this->resolutionItem != nullptr)
				{
					this->resolutionToolStripMenuItem->DropDownItems->Clear();
					delete this->resolutionItem;
				}

				hr = pCapture->FindInterface(&PIN_CATEGORY_CAPTURE,&MEDIATYPE_Video,*ppFilter,IID_IAMStreamConfig,(void**)&pvideoConfig);
				if (FAILED(hr))
				{
					System::Windows::Forms::MessageBox::Show("Get config interface failed.\r\n");
					ShowComError(hr);
					return -1;
				}
				hr = pvideoConfig->GetNumberOfCapabilities(&iCount,&iSize);
				if (FAILED(hr))
				{
					System::Windows::Forms::MessageBox::Show("Get number of config failed.\r\n");
					ShowComError(hr);
					return -1;
				}
				pSCC = new BYTE[iSize];		
				for (int i=0;i<iCount;i++)
				{					
					hr = pvideoConfig->GetStreamCaps(i, &pMt, pSCC);					
					if (hr == S_OK)
					{
						// For FORMAT_VideoInfo
						if (((*pMt).formattype == FORMAT_VideoInfo) &&
						((*pMt).cbFormat >= sizeof(VIDEOINFOHEADER)) &&
						((*pMt).pbFormat != NULL))
						{
							videoInfoHeader = (VIDEOINFOHEADER*)(*pMt).pbFormat;
							switch(videoInfoHeader->bmiHeader.biCompression)
							{
								case BI_RGB:
									bmpFormat = String::Format("RGB");
									break;
								case BI_RLE8:
									bmpFormat = String::Format("RLE8");
									break;
								case BI_RLE4:
									bmpFormat = String::Format("RLE4");
									break;
								case BI_BITFIELDS:
									bmpFormat = String::Format("BITFILEDS");
									break;
								case BI_JPEG:
									bmpFormat = String::Format("JPEG");
									break;
								case BI_PNG:
									bmpFormat = String::Format("PNG");
									break;
								case 0x32595559:
									bmpFormat = String::Format("YUY2");
									break;
								case 0x47504a4d:
									bmpFormat = String::Format("MJPG");
									break;
								default:
									bmpFormat = String::Format("Unknown");
									break;
							}
								// Add the resolution to the "Option->Resolution" menu.
								this->resolutionItem = gcnew ToolStripMenuItem();
								this->resolutionToolStripMenuItem->DropDownItems->AddRange(gcnew cli::array< System::Windows::Forms::ToolStripItem^  >(1) {this->resolutionItem});
								this->resolutionItem->Text = String::Format("{0,-8}{1}{2,-8}{3,6}{4,5}-bit @ {5:F} FPS",
									videoInfoHeader->bmiHeader.biWidth,String::Format("X"),videoInfoHeader->bmiHeader.biHeight,
									bmpFormat,videoInfoHeader->bmiHeader.biBitCount,10000000.0/(float)videoInfoHeader->AvgTimePerFrame);
								this->resolutionItem->Click += gcnew System::EventHandler(this,&Form1::resolutionItem_Click);
								this->resolutionItem->CheckOnClick = false;
								resolList[resolCount++]=(UINT)i;
								resolNum=resolCount;
								if(resolNum>=MAX_RESOL_LIST_NUM)
									break;
						}						
						//else if (((*pMt).formattype == FORMAT_VideoInfo2) &&     //For FORMAT_VideoInfo2
						//((*pMt).cbFormat >= sizeof(VIDEOINFOHEADER2)) &&
						//((*pMt).pbFormat != NULL))
						//{	

						//}
					}	
					else
					{
						System::Windows::Forms::MessageBox::Show("Get media type failed.\r\n");
						ShowComError(hr);
						return -1;
					}			
				}
				if(!SetResolution_WH(ppFilter,320,240)) 
				{
					this->textBox1->AppendText("No specificed resolution (320X240)");
					hr = SetResolution(ppFilter,resolList[0]);				
				}
				if(FAILED(hr))
				{
					System::Windows::Forms::MessageBox::Show("Set resolution failed.\r\n");
					return -1;
				}
								
				delete pSCC;
				_DeleteMediaType(pMt);

				return hr;
		 }
private: int SetResolution_WH(IBaseFilter **ppFilter,UINT width,UINT height){
			 	IAMStreamConfig *pvideoConfig;
				AM_MEDIA_TYPE *pMt;
				VIDEOINFOHEADER* videoInfoHeader = NULL;
				BYTE *pSCC = NULL;
				int iCount = NULL;
				int iSize = NULL;
				
				FILTER_STATE fs;
				hr = pMediaCtrl->GetState(200,(OAFilterState*)&fs);
				if(fs!=State_Stopped)
				{
					pMediaCtrl->Stop();
				}

				hr = pCapture->FindInterface(&PIN_CATEGORY_CAPTURE,&MEDIATYPE_Video,*ppFilter,IID_IAMStreamConfig,(void**)&pvideoConfig);
				if (FAILED(hr))
				{
					System::Windows::Forms::MessageBox::Show("Get config interface failed.\r\n");
					ShowComError(hr);
					return -1;
				}
				hr = pvideoConfig->GetNumberOfCapabilities(&iCount,&iSize);
				pSCC = new BYTE[iSize];				
				if (FAILED(hr))
				{
					System::Windows::Forms::MessageBox::Show("Get number of config failed.\r\n");
					ShowComError(hr);
					return -1;
				}
				for(UINT i=0;i<resolNum;i++)
				{
					hr = pvideoConfig->GetStreamCaps(resolList[i], &pMt, pSCC);
					videoInfoHeader = (VIDEOINFOHEADER*)(*pMt).pbFormat;
					if(videoInfoHeader->bmiHeader.biWidth != width || videoInfoHeader->bmiHeader.biHeight !=height)
					{
						if( i == resolNum-1)
						{
							delete pSCC;
							_DeleteMediaType(pMt);
							return 0;   // Fail
						}
						continue;
					}
					hr = pvideoConfig->SetFormat(pMt);
					this->toolStripStatusLabel2->Text=String::Format("{0}X{1}",videoInfoHeader->bmiHeader.biWidth,videoInfoHeader->bmiHeader.biHeight);
					this->toolStripStatusLabel3->Text=String::Format("{0} bits",videoInfoHeader->bmiHeader.biBitCount);								
					this->toolStripStatusLabel4->Text=String::Format("@{0} FPS",(int)(10000000.0/(float)videoInfoHeader->AvgTimePerFrame));
					
					//hr = pGrabber->SetMediaType(pMt);
					if(FAILED(hr))
					{
						System::Windows::Forms::MessageBox::Show("Set media type failed.\r\n");
						ShowComError(hr);
						return 0;
					}
					
					//Set pictureBox size
					pictureBox1->Width = videoInfoHeader->bmiHeader.biWidth;
					pictureBox1->Height = videoInfoHeader->bmiHeader.biHeight;
					if( dispBMP != nullptr)
					{
						pictureBox1->Image = nullptr;
						delete dispBMP;
					}
					dispBMP = gcnew Bitmap(pictureBox1->Width,pictureBox1->Height,PixelFormat::Format24bppRgb);
					rect =  System::Drawing::Rectangle(0,0,dispBMP->Width,dispBMP->Height);
					pictureBox1->Image = dynamic_cast<Image^>(dispBMP);

					#ifdef DEBUG
					this->textBox1->AppendText("Set format:\r\n");
					this->textBox1->AppendText(String::Format("{0}X{1}:{2}-bit\r\n",videoInfoHeader->bmiHeader.biWidth,videoInfoHeader->bmiHeader.biHeight,videoInfoHeader->bmiHeader.biBitCount));
					#endif
					break;
				}
				delete pSCC;
				_DeleteMediaType(pMt);
				return 1;
		 }
private: int SetResolution(IBaseFilter **ppFilter,UINT streamCapsIndex){
			 	IAMStreamConfig *pvideoConfig;
				AM_MEDIA_TYPE *pMt;
				
				VIDEOINFOHEADER* videoInfoHeader = NULL;
				BYTE *pSCC = NULL;
				int iCount = NULL;
				int iSize = NULL;
				
				FILTER_STATE fs;
				hr = pMediaCtrl->GetState(200,(OAFilterState*)&fs);
				if(fs!=State_Stopped)
				{
					pMediaCtrl->Stop();
				}

				hr = pCapture->FindInterface(&PIN_CATEGORY_CAPTURE,&MEDIATYPE_Video,*ppFilter,IID_IAMStreamConfig,(void**)&pvideoConfig);
				if (FAILED(hr))
				{
					System::Windows::Forms::MessageBox::Show("Get config interface failed.\r\n");
					ShowComError(hr);
					return -1;
				}
				hr = pvideoConfig->GetNumberOfCapabilities(&iCount,&iSize);
				pSCC = new BYTE[iSize];				
				if (FAILED(hr))
				{
					System::Windows::Forms::MessageBox::Show("Get number of config failed.\r\n");
					ShowComError(hr);
					return -1;
				}
				hr = pvideoConfig->GetStreamCaps(streamCapsIndex, &pMt, pSCC);
				videoInfoHeader = (VIDEOINFOHEADER*)(*pMt).pbFormat;
				hr = pvideoConfig->SetFormat(pMt);
				if(FAILED(hr))
				{
					System::Windows::Forms::MessageBox::Show("Set media type failed.\r\n");
					ShowComError(hr);
					return -1;
				}
								
				this->toolStripStatusLabel2->Text=String::Format("{0}X{1}",videoInfoHeader->bmiHeader.biWidth,videoInfoHeader->bmiHeader.biHeight);
				this->toolStripStatusLabel3->Text=String::Format("{0} bits",videoInfoHeader->bmiHeader.biBitCount);
				this->toolStripStatusLabel4->Text=String::Format("@{0} FPS",(int)(10000000.0/(float)videoInfoHeader->AvgTimePerFrame));
				
				//Set pictureBox size
				pictureBox1->Width = videoInfoHeader->bmiHeader.biWidth;
				pictureBox1->Height = videoInfoHeader->bmiHeader.biHeight;
				if( dispBMP != nullptr)
				{
					pictureBox1->Image = nullptr;
					delete dispBMP;
				}
				dispBMP = gcnew Bitmap(pictureBox1->Width,pictureBox1->Height,PixelFormat::Format24bppRgb);
				rect =  System::Drawing::Rectangle(0,0,dispBMP->Width,dispBMP->Height);
				pictureBox1->Image = dynamic_cast<Image^>(dispBMP);

				#ifdef DEBUG
				this->textBox1->AppendText("Set format:\r\n");
				this->textBox1->AppendText(String::Format("{0}: {1}X{2}:{3}-bit\r\n",streamCapsIndex,videoInfoHeader->bmiHeader.biWidth,videoInfoHeader->bmiHeader.biHeight,videoInfoHeader->bmiHeader.biBitCount));
				#endif
				delete pSCC;
				_DeleteMediaType(pMt);
				return hr;
		 }
private: HRESULT InitSampleGrabber(bool oneshot){
			
			hr = pGrabber->SetOneShot(oneshot);
			if (FAILED(hr))
			{				
				ShowFailedMessage(SET_ONE_SHOT_FAILED);
			}
			hr = pGrabber->SetBufferSamples(TRUE);
			if (FAILED(hr))
			{
				ShowFailedMessage(SET_BUFFER_FAILED);
			}			
			return hr;
		 }
private: HRESULT GetOneSample(){
		 

			long evCode;

			InitSampleGrabber(TRUE);
			pMediaCtrl->Run();

			hr = pMediaEv->WaitForCompletion(INFINITE, &evCode);

			long cbBuffer; //Buffer size in byte
			hr = pGrabber->GetCurrentBuffer(&cbBuffer, NULL);
			if (FAILED(hr))
			{
				ShowFailedMessage(GET_BUFFER_FAILED);
			}

			pBuffer = (BYTE*)CoTaskMemAlloc(cbBuffer); 
			if (!pBuffer) 
			{
				hr = E_OUTOFMEMORY;
				goto done;
			}

			hr = pGrabber->GetCurrentBuffer(&cbBuffer, (long*)pBuffer);
			if (FAILED(hr))
			{
				goto done;
			}
			//pMediaCtrl->Stop();

			if (pBuffer != nullptr)
			{
				RenderPictureBox(pBuffer);
			}
#ifdef SAVE_SNAP
			AM_MEDIA_TYPE mt;
			VIDEOINFOHEADER* videoInfoHeader = NULL;
			hr = pGrabber->GetConnectedMediaType(&mt);
			videoInfoHeader = (VIDEOINFOHEADER*)(mt).pbFormat;
				//hr = pvideoConfig->SetFormat(pMt);
			//	this->toolStripStatusLabel2->Text=String::Format("GetConnectedMediaType {0}X{1}",videoInfoHeader->bmiHeader.biWidth,videoInfoHeader->bmiHeader.biHeight);
				//this->toolStripStatusLabel3->Text=String::Format("GetConnectedMediaType {0} bits",videoInfoHeader->bmiHeader.biBitCount);			
			if (FAILED(hr))
			{
				goto done;
			}

			// Examine the format block.
			if ((mt.formattype == FORMAT_VideoInfo) && 
				(mt.cbFormat >= sizeof(VIDEOINFOHEADER)) &&
				(mt.pbFormat != NULL)) 
			{
				VIDEOINFOHEADER *pVih = (VIDEOINFOHEADER*)mt.pbFormat;
		
				WCHAR fileName[16];
				wsprintf(fileName, L"oneshot%d.bmp", oneShotCount++);
				hr = WriteBitmap(fileName, &pVih->bmiHeader, 
					mt.cbFormat - SIZE_PREHEADER, pBuffer, cbBuffer);
			}
			else 
			{
				// Invalid format.
				hr = VFW_E_INVALIDMEDIATYPE; 
			}
			_FreeMediaType(mt);
#endif
		done:
			
			CoTaskMemFree(pBuffer); 
			return hr;
		 }
// Connect output pin to filter.
private: HRESULT ConnectFilters(
    IGraphBuilder *pGraph, 
    IPin *pOut,            // Output pin on the upstream filter.
    IBaseFilter *pDest)    // Downstream filter.
{
    IPin *pIn = NULL;
        
    // Find an input pin on the downstream filter.
    HRESULT hr = FindUnconnectedPin(pDest, PINDIR_INPUT, &pIn);
    if (SUCCEEDED(hr))
    {
        // Try to connect them.
        hr = pGraph->Connect(pOut, pIn);
        pIn->Release();
    }
    return hr;
}
// Connect filter to input pin.
private: HRESULT ConnectFilters(IGraphBuilder *pGraph, IBaseFilter *pSrc, IPin *pIn)
{
    IPin *pOut = NULL;
        
    // Find an output pin on the upstream filter.
    HRESULT hr = FindUnconnectedPin(pSrc, PINDIR_OUTPUT, &pOut);
    if (SUCCEEDED(hr))
    {
        // Try to connect them.
        hr = pGraph->Connect(pOut, pIn);
        pOut->Release();
    }
    return hr;
}
 // Connect filter to filter
private: HRESULT ConnectFilters(IGraphBuilder *pGraph, IBaseFilter *pSrc, IBaseFilter *pDest)
{
    IPin *pOut = NULL;

    // Find an output pin on the first filter.
    HRESULT hr = FindUnconnectedPin(pSrc, PINDIR_OUTPUT, &pOut);
    if (SUCCEEDED(hr))
    {
        hr = ConnectFilters(pGraph, pOut, pDest);
        pOut->Release();
    }
    return hr;
}

private: HRESULT DisconnectFilters(IGraphBuilder *pGraph, IPin *pOut,IBaseFilter *pDest)    // Downstream filter.
{
    IPin *pIn = NULL;
        
    // Find an input pin on the downstream filter.
    HRESULT hr = FindConnectedPin(pDest, PINDIR_INPUT, &pIn);
    if (SUCCEEDED(hr))
    {
        // Try to connect them.
        hr = pGraph->Disconnect(pOut);
		hr = pGraph->Disconnect(pIn);
        pIn->Release();
    }
    return hr;
}
private: HRESULT DisconnectFilters(IGraphBuilder *pGraph, IBaseFilter *pSrc, IPin *pIn)
{
    IPin *pOut = NULL;
        
    // Find an output pin on the upstream filter.
    HRESULT hr = FindUnconnectedPin(pSrc, PINDIR_OUTPUT, &pOut);
    if (SUCCEEDED(hr))
    {
        // Try to connect them.
        hr = pGraph->Connect(pOut, pIn);
        pOut->Release();
    }
    return hr;
}
private: HRESULT DisconnectFilters(IGraphBuilder *pGraph, IBaseFilter *pSrc, IBaseFilter *pDest)
{
    IPin *pOut = NULL;

    // Find an output pin on the first filter.
    HRESULT hr = FindConnectedPin(pSrc, PINDIR_OUTPUT, &pOut);
    if (SUCCEEDED(hr))
    {
        hr = DisconnectFilters(pGraph, pOut, pDest);
        pOut->Release();
    }
    return hr;
}

private: HRESULT FindConnectedPin(IBaseFilter *pFilter, PIN_DIRECTION PinDir, IPin **ppPin)
{
    IEnumPins *pEnum = NULL;
    IPin *pPin = NULL;
    BOOL bFound = FALSE;

    HRESULT hr = pFilter->EnumPins(&pEnum);
    if (FAILED(hr))
    {
        goto done;
    }

    while (S_OK == pEnum->Next(1, &pPin, NULL))
    {
        hr = MatchPin(pPin, PinDir, TRUE, &bFound);
        if (FAILED(hr))
        {
            goto done;
        }
        if (bFound)
        {
            *ppPin = pPin;
            (*ppPin)->AddRef();
            break;
        }
        SafeRelease(&pPin);
    }

    if (!bFound)
    {
        hr = VFW_E_NOT_FOUND;
    }

done:
    SafeRelease(&pPin);
    SafeRelease(&pEnum);
    return hr;
}
		 // Return the first unconnected input pin or output pin.
private: HRESULT FindUnconnectedPin(IBaseFilter *pFilter, PIN_DIRECTION PinDir, IPin **ppPin)
{
    IEnumPins *pEnum = NULL;
    IPin *pPin = NULL;
    BOOL bFound = FALSE;

    HRESULT hr = pFilter->EnumPins(&pEnum);
    if (FAILED(hr))
    {
        goto done;
    }

    while (S_OK == pEnum->Next(1, &pPin, NULL))
    {
        hr = MatchPin(pPin, PinDir, FALSE, &bFound);
        if (FAILED(hr))
        {
            goto done;
        }
        if (bFound)
        {
            *ppPin = pPin;
            (*ppPin)->AddRef();
            break;
        }
        SafeRelease(&pPin);
    }

    if (!bFound)
    {
        hr = VFW_E_NOT_FOUND;
    }

done:
    SafeRelease(&pPin);
    SafeRelease(&pEnum);
    return hr;
}
		 // Match a pin by pin direction and connection state.
private: HRESULT MatchPin(IPin *pPin, PIN_DIRECTION direction, BOOL bShouldBeConnected, BOOL *pResult)
{
    assert(pResult != NULL);

    BOOL bMatch = FALSE;
    BOOL bIsConnected = FALSE;

    HRESULT hr = IsPinConnected(pPin, &bIsConnected);
    if (SUCCEEDED(hr))
    {
        if (bIsConnected == bShouldBeConnected)
        {
            hr = IsPinDirection(pPin, direction, &bMatch);
        }
    }

    if (SUCCEEDED(hr))
    {
        *pResult = bMatch;
    }
    return hr;
}
// Query whether a pin is connected to another pin.
//
// Note: This function does not return a pointer to the connected pin.

HRESULT IsPinConnected(IPin *pPin, BOOL *pResult)
{
    IPin *pTmp = NULL;
    HRESULT hr = pPin->ConnectedTo(&pTmp);
    if (SUCCEEDED(hr))
    {
        *pResult = TRUE;
    }
    else if (hr == VFW_E_NOT_CONNECTED)
    {
        // The pin is not connected. This is not an error for our purposes.
        *pResult = FALSE;
        hr = S_OK;
    }

    SafeRelease(&pTmp);
    return hr;
}
		 // Query whether a pin has a specified direction (input / output)
HRESULT IsPinDirection(IPin *pPin, PIN_DIRECTION dir, BOOL *pResult)
{
    PIN_DIRECTION pinDir;
    HRESULT hr = pPin->QueryDirection(&pinDir);
    if (SUCCEEDED(hr))
    {
        *pResult = (pinDir == dir);
    }
    return hr;
}

// Writes a bitmap file
//  pszFileName:  Output file name.
//  pBMI:         Bitmap format information (including pallete).
//  cbBMI:        Size of the BITMAPINFOHEADER, including palette, if present.
//  pData:        Pointer to the bitmap bits.
//  cbData        Size of the bitmap, in bytes.

HRESULT WriteBitmap(PCWSTR pszFileName, BITMAPINFOHEADER *pBMI, size_t cbBMI,
    BYTE *pData, size_t cbData)
{
    HANDLE hFile = CreateFile(pszFileName, GENERIC_WRITE, 0, NULL, 
        CREATE_ALWAYS, 0, NULL);
    if (hFile == NULL)
    {
        return HRESULT_FROM_WIN32(GetLastError());
    }

    BITMAPFILEHEADER bmf = { };

    bmf.bfType = 'MB';
    bmf.bfSize = cbBMI+ cbData + sizeof(bmf); 
    bmf.bfOffBits = sizeof(bmf) + cbBMI; 

    DWORD cbWritten = 0;
    BOOL result = WriteFile(hFile, &bmf, sizeof(bmf), &cbWritten, NULL);
    if (result)
    {
        result = WriteFile(hFile, pBMI, cbBMI, &cbWritten, NULL);
    }
    if (result)
    {
        result = WriteFile(hFile, pData, cbData, &cbWritten, NULL);
    }

    HRESULT hr = result ? S_OK : HRESULT_FROM_WIN32(GetLastError());

    CloseHandle(hFile);

    return hr;
}

private: int InitPictureBox(){			 

			 /*dispBMP = gcnew Bitmap("InitPictureBox.BMP");
			 sampleBMP = gcnew Bitmap("InitPictureBox2.BMP");*/
			 //dispBMP = gcnew Bitmap(pictureBox1->Width,pictureBox1->Height,PixelFormat::Format24bppRgb);
			 //sampleBMP = gcnew Bitmap("InitPictureBox2.BMP");

			 pictureBox1->Image = dynamic_cast<Image^>(dispBMP);

			 //pictureBox1->Image = gcnew Bitmap("InitPictureBox.BMP");
			 //dispBMP =  dynamic_cast<Bitmap^>(pictureBox1->Image);
			 //sampleBMP = dynamic_cast<Bitmap^>(pictureBox1->Image);

			 #ifdef DEBUG
			 textBox1->AppendText(String::Format( "dispBMP format: {0}\n", dispBMP->PixelFormat));
			 #endif

			 rect =  System::Drawing::Rectangle(0,0,dispBMP->Width,dispBMP->Height);
			 dispBMPData = dispBMP->LockBits(rect,ImageLockMode::ReadWrite, dispBMP->PixelFormat);
			 dispBMP->UnlockBits(dispBMPData);

			 return 0;
		 }
inline int RenderPictureBox(BYTE* pSample){

			 dispBMPData = dispBMP->LockBits(rect,ImageLockMode::ReadWrite, dispBMP->PixelFormat);
			 unsigned char* ptr = (unsigned char*)(void*)dispBMPData->Scan0;
			 int padding0 = dispBMPData->Stride-dispBMPData->Width*3;

			 // Buffer form sample grabber is Y-flip.
			 // The Bitmap data has to scan backward.
			 ptr +=  dispBMPData->Stride*(dispBMPData->Height-1); // Head of the last line
			 for(int j=0;j<dispBMPData->Height;j++)
			 {
				 for(int i=0;i<dispBMPData->Width;i++)
				 {
					 *ptr=*pSample;			//Blue
					 *(ptr+1)=*(pSample+1); //Green
					 *(ptr+2)=*(pSample+2); //Red		
					 ptr+=3;				
					 pSample+=3;
				 }
				 // Back to the next head of the line.
				  ptr = ptr - (dispBMPData->Stride- padding0)-(dispBMPData->Stride- padding0)+padding0; 
			 }

			 dispBMP->UnlockBits(dispBMPData);
			 //dispBMP->RotateFlip(RotateFlipType::RotateNoneFlipY);
			 pictureBox1->Image = dynamic_cast<Image^>(dispBMP);
			 return 0;	
}

// Release the format block for a media type.
void _FreeMediaType(AM_MEDIA_TYPE& mt)
{
    if (mt.cbFormat != 0)
    {
        CoTaskMemFree((PVOID)mt.pbFormat);
        mt.cbFormat = 0;
        mt.pbFormat = NULL;
    }
    if (mt.pUnk != NULL)
    {
        // pUnk should not be used.
        mt.pUnk->Release();
        mt.pUnk = NULL;
    }
}
// Delete a media type structure that was allocated on the heap.
void _DeleteMediaType(AM_MEDIA_TYPE *pmt)
{
    if (pmt != NULL)
    {
        _FreeMediaType(*pmt); 
        CoTaskMemFree(pmt);
    }
}
template <class T> void SafeRelease(T **ppT)
{
    if (*ppT)
    {
        (*ppT)->Release();
        *ppT = NULL;
    }
}
private: System::Void tEST1ToolStripMenuItem_Click(System::Object^  sender, System::EventArgs^  e) {
			 //item0->CheckState = CheckState::Unchecked;
			 ToolStripMenuItem^ item0 = (ToolStripMenuItem^)this->devicesToolStripMenuItem->DropDownItems[0];
			 ToolStripMenuItem^ item1 = (ToolStripMenuItem^)this->devicesToolStripMenuItem->DropDownItems[1];
			 ToolStripMenuItem^ item2 = (ToolStripMenuItem^)this->devicesToolStripMenuItem->DropDownItems[2];
			 item0->CheckState = CheckState::Unchecked;
			 item1->CheckState = CheckState::Unchecked;
			 item2->CheckState = CheckState::Unchecked;	 
			 this->textBox1->AppendText(devNum.ToString());
		 }

// For DEBUG/////////////////////////////////////////////////////////////////
HRESULT AddToRot(IUnknown *pUnkGraph, DWORD *pdwRegister) 
{
    IMoniker * pMoniker = NULL;
    IRunningObjectTable *pROT = NULL;

    if (FAILED(GetRunningObjectTable(0, &pROT))) 
    {
        return E_FAIL;
    }
    
    const size_t STRING_LENGTH = 256;

    WCHAR wsz[STRING_LENGTH];
 
   StringCchPrintfW(
        wsz, STRING_LENGTH, 
        L"FilterGraph %08x pid %08x", 
        (DWORD_PTR)pUnkGraph, 
        GetCurrentProcessId()
        );
    
    HRESULT hr = CreateItemMoniker(L"!", wsz, &pMoniker);
    if (SUCCEEDED(hr)) 
    {
        hr = pROT->Register(ROTFLAGS_REGISTRATIONKEEPSALIVE, pUnkGraph,
            pMoniker, pdwRegister);
        pMoniker->Release();
    }
    pROT->Release();
    
    return hr;
}
void RemoveFromRot(DWORD pdwRegister)
{
    IRunningObjectTable *pROT;
    if (SUCCEEDED(GetRunningObjectTable(0, &pROT))) {
        pROT->Revoke(pdwRegister);
        pROT->Release();
    }
}
HRESULT SaveGraphFile(IGraphBuilder *pGraph, WCHAR *wszPath) 
{
    const WCHAR wszStreamName[] = L"ActiveMovieGraph"; 
    HRESULT hr;
    
    IStorage *pStorage = NULL;
    hr = StgCreateDocfile(
        wszPath,
        STGM_CREATE | STGM_TRANSACTED | STGM_READWRITE | STGM_SHARE_EXCLUSIVE,
        0, &pStorage);
    if(FAILED(hr)) 
    {
        return hr;
    }

    IStream *pStream;
    hr = pStorage->CreateStream(
        wszStreamName,
        STGM_WRITE | STGM_CREATE | STGM_SHARE_EXCLUSIVE,
        0, 0, &pStream);
    if (FAILED(hr)) 
    {
        pStorage->Release();    
        return hr;
    }

    IPersistStream *pPersist = NULL;
    pGraph->QueryInterface(IID_IPersistStream, (void**)&pPersist);
    hr = pPersist->Save(pStream, TRUE);
    pStream->Release();
    pPersist->Release();
    if (SUCCEEDED(hr)) 
    {
        hr = pStorage->Commit(STGC_DEFAULT);
    }
    pStorage->Release();
    return hr;
}
private: void ShowFailedMessage(unsigned char failID)
{
	switch(failID)
	{
		case CREATE_GRAPH_FAILED:
				  System::Windows::Forms::MessageBox::Show("CREATE_GRAPH_BUILDER_FAILED");   
				  break;
		case CREATE_CAPTURE_FAILED:
				  System::Windows::Forms::MessageBox::Show("CREATE_CAPTURE_GRAPH_BUILDER_FAILED");   
				  break;
		case OBTAIN_IF_MC_FAILED:
				  System::Windows::Forms::MessageBox::Show("OBTAIN_IF_MC_FAILED");   
				  break;
  		case OBTAIN_IF_VW_FAILED:
				  System::Windows::Forms::MessageBox::Show("OBTAIN_IF_VW_FAILED");   
				  break;
  		case OBTAIN_IF_ME_FAILED:
				  System::Windows::Forms::MessageBox::Show("OBTAIN_IF_ME_FAILED");   
				  break;
		case SET_WIN_HANDL_FAILED:
				  System::Windows::Forms::MessageBox::Show("SET_WIN_HANDL_FAILED");   
				  break;
		case CAPTURE_INIT_FAILED:
				  System::Windows::Forms::MessageBox::Show("CAPTURE_INIT_FAILED");   
				  break;
		case NO_VIDEO_DEV:
				  System::Windows::Forms::MessageBox::Show("NO_VIDEO_DEV");   
				  break;
		case CREATE_ENUM_FAILED:
				  System::Windows::Forms::MessageBox::Show("CREATE_ENUM_FAILED");   
				  break;
		case CREATE_BINDCTX_FAILED:
				  System::Windows::Forms::MessageBox::Show("CREATE_BINDCTX_FAILED");   
				  break;
		case GET_NAME_FAILED:
				  System::Windows::Forms::MessageBox::Show("GET_NAME_FAILED");   
				  break;
		case BIND_TO_OBJECT_ERROR:
				  System::Windows::Forms::MessageBox::Show("BIND_TO_OBJECT_ERROR");   
				  break;
		case REMOVE_CAPTURE_FILTER_FAILED:
				  System::Windows::Forms::MessageBox::Show("REMOVE_CAPTURE_FILTER_FAILED");   
				  break;
		default:
				System::Windows::Forms::MessageBox::Show("Undefined Error");				
				break;
	}
	Application::ExitThread();
	Application::Exit();
}
private: void ShowFailedMessage(unsigned char failID,HRESULT hr)
{

	_com_error err(hr);
	LPCTSTR errMsg = err.ErrorMessage();
	String^ str = gcnew System::String(errMsg);
	switch(failID)
	{
		case CREATE_GRAPH_FAILED:
				  System::Windows::Forms::MessageBox::Show("CREATE_GRAPH_FAILED",str);   
				  break;
		case CREATE_CAPTURE_FAILED:
				  System::Windows::Forms::MessageBox::Show("CREATE_CAPTURE_GRAPH_BUILDER_FAILED",str);   
				  break;
		case OBTAIN_IF_MC_FAILED:
				  System::Windows::Forms::MessageBox::Show("OBTAIN_IF_MC_FAILED",str);   
				  break;
  		case OBTAIN_IF_VW_FAILED:
				  System::Windows::Forms::MessageBox::Show("OBTAIN_IF_VW_FAILED",str);   
				  break;
  		case OBTAIN_IF_ME_FAILED:
				  System::Windows::Forms::MessageBox::Show("OBTAIN_IF_ME_FAILED",str);   
				  break;
		case SET_WIN_HANDL_FAILED:
				  System::Windows::Forms::MessageBox::Show("SET_WIN_HANDL_FAILED",str);   
				  break;
		case CAPTURE_INIT_FAILED:
				  System::Windows::Forms::MessageBox::Show("CAPTURE_INIT_FAILED",str);   
				  break;
		case NO_VIDEO_DEV:
				  System::Windows::Forms::MessageBox::Show("NO_VIDEO_DEV",str);   
				  break;
		case CREATE_ENUM_FAILED:
				  System::Windows::Forms::MessageBox::Show("CREATE_ENUM_FAILED",str);   
				  break;
		case CREATE_BINDCTX_FAILED:
				  System::Windows::Forms::MessageBox::Show("CREATE_BINDCTX_FAILED",str);   
				  break;
		case GET_NAME_FAILED:
				  System::Windows::Forms::MessageBox::Show("GET_NAME_FAILED",str);
				  this->textBox1->AppendText(str);
				  break;
		case BIND_TO_OBJECT_ERROR:
				  System::Windows::Forms::MessageBox::Show("BIND_TO_OBJECT_ERROR",str);   
				  break;
		case E_POINTER:
				  System::Windows::Forms::MessageBox::Show("E_POINTER",str);   
				  break;
		case E_INVALIDARG:
				  System::Windows::Forms::MessageBox::Show("E_INVALIDARG",str);   
				  break;
		case E_FAIL:
				  System::Windows::Forms::MessageBox::Show("E_FAIL",str);   
				  break;


		default:
				System::Windows::Forms::MessageBox::Show("Undefined Error",str);
				this->textBox1->AppendText(str);
				break;
	}
	/*Application::ExitThread();
	Application::Exit();*/

}
private: void ShowComError(HRESULT hr){
			_com_error err(hr);
			LPCTSTR errMsg = err.ErrorMessage();
			String^ str = gcnew System::String(errMsg);
			System::Windows::Forms::MessageBox::Show(str);
		 }
///////////////////////////////////////////////////////////////////

private: System::Void Timer1_ISR(System::Object^  sender, System::EventArgs^  e)
   {
      //timer1->Stop();

	  textBox1->AppendText(String::Format("ISR: {0}\r\n",dCount++));
	  GetOneSample();
   }
private: System::Void button1_Click(System::Object^  sender, System::EventArgs^  e) {

			 textBox1->AppendText("Take one shot\r\n.");
			 //GetOneSample();
			 if(gsampleGrabberCB->pImagBuffer!=0)
			 {
#ifdef SAVE_SNAP
			AM_MEDIA_TYPE mt;
			VIDEOINFOHEADER* videoInfoHeader = NULL;
			hr = pGrabber->GetConnectedMediaType(&mt);
			videoInfoHeader = (VIDEOINFOHEADER*)(mt).pbFormat;


			// Examine the format block.
			if ((mt.formattype == FORMAT_VideoInfo) && 
				(mt.cbFormat >= sizeof(VIDEOINFOHEADER)) &&
				(mt.pbFormat != NULL)) 
			{
				VIDEOINFOHEADER *pVih = (VIDEOINFOHEADER*)mt.pbFormat;
		
				WCHAR fileName[16];
				wsprintf(fileName, L"oneshot%d.bmp", oneShotCount++);
				hr = WriteBitmap(fileName, &pVih->bmiHeader, 
					mt.cbFormat - SIZE_PREHEADER, gsampleGrabberCB->pImagBuffer, pictureBox1->Width*pictureBox1->Height*3);
			}
			else 
			{
				// Invalid format.
				hr = VFW_E_INVALIDMEDIATYPE; 
			}
			_FreeMediaType(mt);
#endif
			 }

		 }
private: System::Void button2_Click(System::Object^  sender, System::EventArgs^  e) {

#ifdef MMTIMER

			if(!timerIsRunning)
			{
				mmResult = timeBeginPeriod(1); 
				if(mmResult == TIMERR_NOCANDO )
				{
					textBox1->AppendText("Timer resolution specified in uPeriod is out of range.");				 
				}
				else
				{
					this->textBox1->AppendText(String::Format("wTimerRes = {0}.\r\n",1));				 
				}
				timerEventHandler = gcnew TimerEventHandler(this, &Form1::MMTimer_ISR);
				mmtimerIntval = Convert::ToUInt32(this->textBox2->Text);
				if(mmtimerIntval == 0)
				{
					mmtimerIntval = 100;
					this->textBox2->Text = String::Format("{0)",mmtimerIntval);
				}				
				mmTimerID = timeSetEvent(mmtimerIntval,0,timerEventHandler,IntPtr::Zero,TIME_PERIODIC);
				this->button2->Text = "Pause";
				timerIsRunning = true;
			}
			else
			{
				timeKillEvent(mmTimerID);
				timeEndPeriod(1);
				timerIsRunning = false;
				this->button2->Text = "Run";
			}
#else
			 if(!timerIsRunning)
			 {
				 // Set the ISR to the timer.
				 this->timer1->Tick += gcnew EventHandler(this,&camera_dip::Form1::Timer1_ISR );
				 this->timer1->Interval = 500;
				 this->timer1->Start();
				 timerIsRunning = true;
			 }
			 else
			 {
				 this->timer1->Stop();
				 button2->Text = "Run";
				 timerIsRunning = false;
				 button2->Text = "Pause";
			 }
#endif
		 }
private: System::Void button3_Click(System::Object^  sender, System::EventArgs^  e) {

			 InitSampleGrabber(false); // Continuous mode
			 if(!grabberCBIsRunning)
			 {					 
				 pGrabber->SetCallback((ISampleGrabberCB *)gsampleGrabberCB,1);
				 pMediaCtrl->Run();
				 grabberCBIsRunning = true;
				 button3->Text = "Disable Camera";
			 }
			 else
			 {
				 pMediaCtrl->Stop();
				 pGrabber->SetCallback(0,1);
				 grabberCBIsRunning = false;				 
				 textBox1->AppendText(String::Format("{0}\r\n",gsampleGrabberCB->sampleCount));
				 gsampleGrabberCB->sampleCount=0;
				 button3->Text = "Enable Camera";
			 }
		 }

private: inline System::Void MMTimer_ISR( int id, int msg, IntPtr user, int dw1, int dw2 )
   {
	   this->BeginInvoke(gcnew TestEventHandler(this, &Form1::ShowTick));
   }
private: inline void ShowTick()	// Dialog updating should be implemented here.
{
	tickCount2++;
	this->textBox1->AppendText(String::Format("Tick:{0:0000}\tgrabberCB:{1:0000}\r\n",tickCount++,gsampleGrabberCB->sampleCount));
	if((int)tickCount%1000 ==0)
		this->textBox1->Clear();

	if(mmtimerIntval*tickCount2 >= 1000)
	{
		this->label4->Text = String::Format("{0:0.##}",(1000.0*(float)gsampleGrabberCB->sampleCount/(float)(mmtimerIntval*tickCount2)));
		gsampleGrabberCB->sampleCount = 0;
		tickCount2 = 0;
	}

	if(gsampleGrabberCB->pImagBuffer!=0)
	   {
		   	#ifdef DEBUG			
			 gstopWatch->Start();
			#endif
			RenderPictureBox(gsampleGrabberCB->pImagBuffer);
			#ifdef DEBUG
			 gstopWatch->Stop();			 
			 textBox1->AppendText(String::Format("Render time:{0:D} ms\r\n",gstopWatch->ElapsedMilliseconds));		
			 gstopWatch->Reset();			
			#endif
	   }
}
private: System::Void capturePinToolStripMenuItem_Click(System::Object^  sender, System::EventArgs^  e) {
			 			 IAMStreamConfig *pvideoConfig;
			 ISpecifyPropertyPages *pProp;
			 IPin *pPin = NULL;

			 pMediaCtrl->Stop();
			 hr = DisconnectFilters(pGraph, pGrabberF, pNullF);	
			 hr = DisconnectFilters(pGraph, pCaptureFilter, pGrabberF);

			 hr = pCapture->FindInterface(&PIN_CATEGORY_CAPTURE,&MEDIATYPE_Video,pCaptureFilter,IID_IAMStreamConfig,(void**)&pvideoConfig);
			 if (FAILED(hr))
			 {
				 System::Windows::Forms::MessageBox::Show("Get config interface failed.\r\n");
				 ShowComError(hr);				
			 }

			 hr = FindUnconnectedPin(pCaptureFilter, PINDIR_OUTPUT, &pPin);
			 hr = pPin->QueryInterface( IID_ISpecifyPropertyPages, (void **)&pProp );
			 pPin->Release();
			 //hr = pvideoConfig->QueryInterface(IID_ISpecifyPropertyPages, (void **)&pProp);
			 if (SUCCEEDED(hr)) 
			 {
					
					// Show the page. 
					CAUUID caGUID;
					pProp->GetPages(&caGUID);
					pProp->Release();

					OleCreatePropertyFrame(
						0,                   // Parent window
						0, 0,                   // Reserved
						L"Filter",     // Caption for the dialog box
						1,                      // Number of objects (just the filter)
						(IUnknown **)&pPin,            // Array of object pointers. 
						caGUID.cElems,          // Number of property pages
						caGUID.pElems,          // Array of property page CLSIDs
						0,                      // Locale identifier
						0, NULL                 // Reserved
					);
						// Clean up.
//					pFilterUnk->Release();
//					FilterInfo.pGraph->Release(); 
					CoTaskMemFree(caGUID.pElems);
					hr = BuildGraph(&pCaptureFilter);


					AM_MEDIA_TYPE mt;
					VIDEOINFOHEADER* videoInfoHeader = NULL;
					hr = pGrabber->GetConnectedMediaType(&mt);
					videoInfoHeader = (VIDEOINFOHEADER*)(mt).pbFormat;

					this->toolStripStatusLabel2->Text=String::Format("{0}X{1}",videoInfoHeader->bmiHeader.biWidth,videoInfoHeader->bmiHeader.biHeight);
					this->toolStripStatusLabel3->Text=String::Format("{0} bits",videoInfoHeader->bmiHeader.biBitCount);								
					this->toolStripStatusLabel4->Text=String::Format("@{0} FPS",(int)(10000000.0/(float)videoInfoHeader->AvgTimePerFrame));

					//Set pictureBox size
					pictureBox1->Width = videoInfoHeader->bmiHeader.biWidth;
					pictureBox1->Height = videoInfoHeader->bmiHeader.biHeight;
					if( dispBMP != nullptr)
					{
						pictureBox1->Image = nullptr;
						delete dispBMP;
					}
					dispBMP = gcnew Bitmap(pictureBox1->Width,pictureBox1->Height,PixelFormat::Format24bppRgb);
					rect =  System::Drawing::Rectangle(0,0,dispBMP->Width,dispBMP->Height);
					pictureBox1->Image = dynamic_cast<Image^>(dispBMP);
			 }
		 }
private: System::Void cameraPropertyToolStripMenuItem_Click(System::Object^  sender, System::EventArgs^  e) {

			 ISpecifyPropertyPages *pProp;
				hr = pCaptureFilter->QueryInterface(IID_ISpecifyPropertyPages, (void **)&pProp);
				if (SUCCEEDED(hr)) 
				{
					// Get the filter's name and IUnknown pointer.
					FILTER_INFO FilterInfo;
					hr = pCaptureFilter->QueryFilterInfo(&FilterInfo); 
					IUnknown *pFilterUnk;
					pCaptureFilter->QueryInterface(IID_IUnknown, (void **)&pFilterUnk);

					// Show the page. 
					CAUUID caGUID;
					pProp->GetPages(&caGUID);
					pProp->Release();
					OleCreatePropertyFrame(
						0,                   // Parent window
						0, 0,                   // Reserved
						FilterInfo.achName,     // Caption for the dialog box
						1,                      // Number of objects (just the filter)
						&pFilterUnk,            // Array of object pointers. 
						caGUID.cElems,          // Number of property pages
						caGUID.pElems,          // Array of property page CLSIDs
						0,                      // Locale identifier
						0, NULL                 // Reserved
					);
						// Clean up.
					pFilterUnk->Release();
					FilterInfo.pGraph->Release(); 
					CoTaskMemFree(caGUID.pElems);
				}
		 }
};






}


