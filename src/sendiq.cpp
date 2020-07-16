#include <unistd.h>
#include "librpitx/src/librpitx.h"
#include "stdio.h"
#include <cstring>
#include <signal.h>
#include <stdlib.h>

sig_atomic_t  running;

#define PROGRAM_VERSION "0.2"

void print_usage(void)
{

fprintf(stderr,\
"\nsendiq -%s\n\
Usage:\nsendiq [-i File Input][-s Samplerate][-l] [-f Frequency] [-h Harmonic number] \n\
-i            path to File Input \n\
-s            SampleRate 10000-250000 \n\
-f float      central frequency Hz(50 kHz to 1500 MHz),\n\
-m int        shared memory token,\n\
-d            dds mode,\n\
-p            ppm clock correction,\n\
-a            power level (0.00 to 7.00),\n\
-l            loop mode for file input\n\
-h            Use harmonic number n\n\
-t            IQ type (i16 default) {i16,u8,float,double}\n\
-?            help (this help).\n\
\n",\
PROGRAM_VERSION);

} /* end function print_usage */

static void
terminate(int num)
{
    fprintf(stderr,"Caught signal - Terminating %x\n",num);
    running = 0;
   
}

#define MAX_SAMPLERATE 200000

int main(int argc, char* argv[]) {
  running = 1;
  bool  fdds=false;            //operate as a DDS
	int a;
	int anyargs = 1;
	float SetFrequency=434e6;
	float SampleRate=48000;
	bool loop_mode_flag=false;
	char* FileName=NULL;
	int Harmonic=1;
	enum {typeiq_i16,typeiq_u8,typeiq_float,typeiq_double};
	int InputType=typeiq_i16;
	int Decimation=1;

  bool ppmSet = false;
	float ppm = 0.0;
  float drivedds=0.1;          //drive level
        
	while(1)
	{
		a = getopt(argc, argv, "i:f:s:m:p:h:ldt:");
	
		if(a == -1) 
		{
			if(anyargs) break;
			else a='h'; //print usage and exit
		}
		anyargs = 1;	

		switch(a)
		{
		case 'i': // File name
			FileName = optarg;
			break;
		case 'd': // dds mode
			fdds=true;
			break;
    case 'p':
			ppm=atof(optarg);
      ppmSet = true;
      break;
		case 'a': // driver level (power)
			drivedds=atof(optarg);
			if (drivedds<0.0) {drivedds=0.0;}
			if (drivedds>7.0) {drivedds=7.0;}
			break;
		case 'f': // Frequency
			SetFrequency = atof(optarg);
			break;
		case 's': // SampleRate (Only needeed in IQ mode)
			SampleRate = atoi(optarg);
			if(SampleRate>MAX_SAMPLERATE) 
			{
				for(int i=2;i<12;i++) //Max 10 times samplerate
				{
					if(SampleRate/i<MAX_SAMPLERATE) 
					{
						SampleRate=SampleRate/i;
						Decimation=i;
						break;
					}
				}
				if(Decimation==1)
				{
					 fprintf(stderr,"SampleRate too high : >%d sample/s",10*MAX_SAMPLERATE);
					 exit(1);
				} 
				else
				{
					fprintf(stderr,"Warning samplerate too high, decimation by %d will be performed",Decimation);	 
				}
			};
			break;
		case 'h': // help
			Harmonic=atoi(optarg);
			break;
		case 'l': // loop mode
			loop_mode_flag = true;
			break;
		case 't': // inout type
			if(strcmp(optarg,"i16")==0) InputType=typeiq_i16;
			if(strcmp(optarg,"u8")==0) InputType=typeiq_u8;
			if(strcmp(optarg,"float")==0) InputType=typeiq_float;
			if(strcmp(optarg,"double")==0) InputType=typeiq_double;
			break;
		case -1:
        	break;
		case '?':
			if (isprint(optopt) )
 			{
 				fprintf(stderr, "sendiq: unknown option `-%c'.\n", optopt);
 			}
			else
			{
				fprintf(stderr, "sendiq: unknown option character `\\x%x'.\n", optopt);
			}
			print_usage();

			exit(1);
			break;			
		default:
			print_usage();
			exit(1);
			break;
		}/* end switch a */
	}/* end while getopt() */

	if(FileName==NULL) {fprintf(stderr,"Need an input\n");exit(1);}
	
   // TODO - make me a common code
	 for (int i = 0; i < 64; i++) {
        struct sigaction sa;

        std::memset(&sa, 0, sizeof(sa));
        sa.sa_handler = terminate;
        sigaction(i, &sa, NULL);
    }

	FILE *iqfile=NULL;
	if(strcmp(FileName,"-")==0)
		iqfile=fopen("/dev/stdin","rb");
	else	
		iqfile=fopen(FileName	,"rb");
	if (iqfile==NULL) 
	{	
		printf("input file issue\n");
		exit(0);
	}

	#define IQBURST 4000
	int FifoSize=IQBURST*4;
  float * ppmPtr = NULL;
  if (ppmSet) {
    ppmPtr = &ppm;
  }
  
	iqdmasync iqtest(SetFrequency,SampleRate,14,FifoSize,MODE_IQ, ppmPtr);
	iqtest.SetPLLMasterLoop(3,4,0);

  if (fdds==true) {           //if instructed to operate as DDS start with carrier, otherwise I/Q mode it is
     iqtest.ModeIQ=MODE_FREQ_A;
  }

	std::complex<float> CIQBuffer[IQBURST];	
	while(running == 1)
	{
		
			int CplxSampleNumber=0;
			switch(InputType)
			{
				case typeiq_i16:
				{
					static short IQBuffer[IQBURST*2];
					int nbread=fread(IQBuffer,sizeof(short),IQBURST*2,iqfile);
					if(nbread>0)
					{
						for(int i=0;i<nbread/2;i++)
						{
							if(i%Decimation==0)
							{		
								CIQBuffer[CplxSampleNumber++]=std::complex<float>(IQBuffer[i*2]/32768.0,IQBuffer[i*2+1]/32768.0); 
							}
						}
					}
					else 
					{
						printf("End of file\n");
						if(loop_mode_flag)
						fseek ( iqfile , 0 , SEEK_SET );
						else
							running=0;
					}
					
				}
				break;
				case typeiq_u8:
				{
					static unsigned char IQBuffer[IQBURST*2];
					int nbread=fread(IQBuffer,sizeof(unsigned char),IQBURST*2,iqfile);
					
					if(nbread>0)
					{
						for(int i=0;i<nbread/2;i++)
						{
							if(i%Decimation==0)
							{	
								CIQBuffer[CplxSampleNumber++]=std::complex<float>((IQBuffer[i*2]-127.5)/128.0,(IQBuffer[i*2+1]-127.5)/128.0);
										
							}		 
						}
					}
					else 
					{
						printf("End of file\n");
						if (loop_mode_flag) {
  						fseek ( iqfile , 0 , SEEK_SET );
            } else {
							running=0;
            }
					}
				}
				break;
				case typeiq_float:
				{
					static float IQBuffer[IQBURST*2];
					int nbread=fread(IQBuffer,sizeof(float),IQBURST*2,iqfile);
					if(nbread>0)
					{
						for(int i=0;i<nbread/2;i++) {

							if (iqtest.ModeIQ==MODE_FREQ_A) {  //if into Frequency-Amplitude mode then only drive a constant carrier
     				    IQBuffer[i*2]=10.0;            //should be 10 Hz 
                IQBuffer[i*2+1]=drivedds;      //at the defined drive level
                          				}
// *---------------------------------------------------------------------------------------------------------------------------------------------
							if(i%Decimation==0) {
								CIQBuffer[CplxSampleNumber++]=std::complex<float>(IQBuffer[i*2],IQBuffer[i*2+1]);
							}
						}
					} else {
						printf("End of file\n");
						if(loop_mode_flag)
						fseek ( iqfile , 0 , SEEK_SET );
						else
							running=0;
					}
				}
				break;	
				case typeiq_double:
				{
					static double IQBuffer[IQBURST*2];
					int nbread=fread(IQBuffer,sizeof(double),IQBURST*2,iqfile);

					if(nbread>0)
					{
						for(int i=0;i<nbread/2;i++)
						{
							if(i%Decimation==0)
							{	
								CIQBuffer[CplxSampleNumber++]=std::complex<float>(IQBuffer[i*2],IQBuffer[i*2+1]);
										
							}		 
						}
					}
					else 
					{
						printf("End of file\n");
						if(loop_mode_flag)
						fseek ( iqfile , 0 , SEEK_SET );
						else
							running=0;
					}
				}
				break;	
			
		}
		iqtest.SetIQSamples(CIQBuffer,CplxSampleNumber,Harmonic);
	}

	iqtest.stop();

}	

