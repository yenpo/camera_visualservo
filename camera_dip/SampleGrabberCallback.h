
namespace SampleGrabber_CB
{
		public class SampleGrabberCallback : ISampleGrabberCB
		{
			public:
			SampleGrabberCallback()
			{
				this->sampleCount = 0;
				this->pImagBuffer = 0;				
			}

			// Fake referance counting. 
			STDMETHODIMP_(ULONG) AddRef() { return 1; } 
			STDMETHODIMP_(ULONG) Release() { return 2; } 
			STDMETHODIMP QueryInterface(REFIID riid, void **ppvObject) 
			{ 
				if (NULL == ppvObject) return E_POINTER; 
				if (riid == __uuidof(IUnknown)) 
				{ 
					*ppvObject = static_cast<IUnknown*>(this); 
					 return S_OK; 
				} 
				if (riid == __uuidof(ISampleGrabberCB)) 
				{ 
					*ppvObject = static_cast<ISampleGrabberCB*>(this); 
					 return S_OK; 
				} 
				return E_NOTIMPL; 
			} 

			public:	int sampleCount;
			public:	double sampleTime;
			public:	BYTE* pImagBuffer;			

			STDMETHODIMP SampleCB(double Time, IMediaSample *pSample)
			{
				return E_NOTIMPL;
			}
			STDMETHODIMP BufferCB(double SampleTime,  BYTE *pBuffer,  long BufferLen)
			{					
				this->sampleCount++;
				this->sampleTime = SampleTime;
				this->pImagBuffer = pBuffer;
				return 0;
			}
		};
}