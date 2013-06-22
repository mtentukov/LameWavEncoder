/*gcc -o lameWav2mp3  tools.c lameWav2mp3.c -pthread -Xlinker -Bstatic -lmp3lame -lm -Xlinker -Bdynamic*/

/*
    M.Tentyukov, 22 Apr. 2013
*/
/*
   A very simple demo program, almost without error checking and adjustable
   parameters.  It accepts one cmd - line parameter -- a directory name -- and
   scans it looking for the files with a .wav extension (case
   insensitive). All found WAV files are tested for the audio format, only pcm
   encoded WAV files with valid fmt 16 are processed, others are ignored; the
   WAV file header must satisfy the structure described in the following WEB
   padges:

     http://www-mmsp.ece.mcgill.ca/documents/AudioFormats/WAVE/WAVE.html
     https://ccrma.stanford.edu/courses/422/projects/WaveFormat/

   According to my experiments, time of encoding even short files is much
   longer (order of magnitude), than simple copying buffers form file to
   file. So, the simplest way to parallelize the process seems to be just to
   allow a worker completely encode one file buffer by buffer.

   The program was tested under Linux on 8-core Intel server, it was also
   compiled with VS 2008 and run on a laptop with installed Pthreads-w32
   2.9.1(2012-05-27) http://sourceware.org/pthreads-win32/

   The idea of a program is rather straightforward: the master (just a main
   program) starts a pool of workers and scans a specified directory. All
   found the WAV file names are queued in a single FIFO queue while workers
   are "eating" these entries from the other side of the queue. When a worker
   gets the file name, it opens the corresponding file, reads a header and, if
   it is a correct PCM, encodes the content into the .mp3 file.
*/

#include <stdio.h>
#include <string.h>
#ifdef _WIN32
#include <lame.h>
#else
#include <inttypes.h>
#include <lame/lame.h>
#endif
#include <stdlib.h>

#include <semaphore.h>
#include <pthread.h>

#include "tools.h"

/*
uncomment this macro to protect existing .mp3 files:
#define DO_NOT_OVERRIDE 1
*/
/*samples to read:*/
#define PCM_SIZE  8192
/*length of encoded buffer to write:*/
#define MP3_SIZE  PCM_SIZE*2 

/*will be determined in main()*/
static int g_cpuNumber = 1;
/*will be equal to number of CPU * 1.5 :*/
static int g_nWorkers = 0;

/*The file names queue:*/
#define QFL_TYPE char*
#define QFL_PREFIX txt_
#include "queue.h"
/*see detailed explanations in a file "queue.h"*/

typedef pthread_t pool_t ;

pool_t *g_pool = NULL;

/*Just for fun:*/
static unsigned g_totalConverted = 0;
static pthread_mutex_t g_mTotalConverted = PTHREAD_MUTEX_INITIALIZER;

static txt_qFL_t  g_qFileName = QFL0;
static pthread_mutex_t g_mFileName = PTHREAD_MUTEX_INITIALIZER;
static sem_t g_sFileName;

/*auxiliary:*/
static sem_t g_sAllThreadsReady;
static pthread_mutex_t g_mEndScan = PTHREAD_MUTEX_INITIALIZER;

static char g_pathname[MAX_PATH_LENGTH];
static int g_pathnameLength = MAX_PATH_LENGTH -1;

/*for a pcm file header:*/
typedef struct
{
  char      RIFF[5];/*4+1 for trailing '\0'*/
  int32_t   chunkSize;     
  char      WAVE[5];/*4+1 for trailing '\0'*/
  char      FMT[5];/*4+1 for trailing '\0'*/
  int32_t   subchunk1Size;
  int16_t   AudioFormat;
  int16_t   NumChannels;
  int32_t   sampleRate;
  int32_t   byteRate;
  int16_t   blockAlign;
  int16_t   bitsPerSample;
  char      subchunk2ID[5];/*4+1 for trailing '\0'*/
  int32_t   subchunk2Size;
}wav_hdr_t;

/*reading a WAV header*/
int getPcmHeader(wav_hdr_t *hdr, FILE *f, int checkSrtucture)
{
   /*Avoid using #pragma's and packed structure so just read field by field:*/
   fread(&(hdr->RIFF),1,4,f);hdr->RIFF[4] = '\0';
   fread(&(hdr->chunkSize),sizeof(int32_t),1,f);
   fread(&(hdr->WAVE),1,4,f);hdr->WAVE[4] = '\0';
   fread(&(hdr->FMT),1,4,f);hdr->FMT[4]='\0';
   fread(&(hdr->subchunk1Size),sizeof(int32_t),1,f);
   fread(&(hdr->AudioFormat),sizeof(int16_t),1,f);
   fread(&(hdr->NumChannels),sizeof(int16_t),1,f);
   fread(&(hdr->sampleRate),sizeof(int32_t),1,f);
   fread(&(hdr->byteRate),sizeof(int32_t),1,f);
   fread(&(hdr->blockAlign),sizeof(int16_t),1,f);
   fread(&(hdr->bitsPerSample),sizeof(int16_t),1,f);
   fread(&(hdr->subchunk2ID),1,4,f);hdr->subchunk2ID[4] = '\0';
   fread(&(hdr->subchunk2Size),sizeof(int32_t),1,f);
   if (0 != checkSrtucture)
   {
      if (0 != strcmp(hdr->RIFF, "RIFF"))
      {
         return -1;
      }
      if (0 != strcmp(hdr->WAVE, "WAVE"))
      {
         return -2;
      }
      if (0 != strcmp(hdr->FMT, "fmt "))
      {
         return -3;
      }
      if (0 != strcmp(hdr->subchunk2ID, "data"))
      {
         return -4;
      }
      if (16 != hdr->subchunk1Size)
      {
         return -5;
      }
      if (1 != hdr->AudioFormat)
      {
         return -6;
      }
      if ( 2 < hdr->NumChannels)
      {
         return -7;
      }
      if (20 != hdr->chunkSize - hdr->subchunk1Size - hdr->subchunk2Size)
      {
         return -8;
      }
   }/*if (0 != checkSrtucture)*/
   return hdr->chunkSize;
}/*getPcmHeader*/

void prnPcmHeader(wav_hdr_t *hdr)
{
   unblockedMessage("RIFF header     :%s\n", hdr->RIFF);
   unblockedMessage("Chunk size      :%" PRId32 "\n", hdr->chunkSize);
   unblockedMessage("WAVE header     :%s\n", hdr->WAVE);
   unblockedMessage("FMT             :%s\n", hdr->FMT);
   unblockedMessage("Subchunk1 size  :%" PRId32 "\n", hdr->subchunk1Size);
   unblockedMessage("Audio format    :%" PRId16 "\n", hdr->AudioFormat);
   unblockedMessage("Num of chans    :%" PRId16 "\n", hdr->NumChannels);
   unblockedMessage("Samples per sec :%" PRId32 "\n", hdr->sampleRate);
   unblockedMessage("Bytes per sec   :%" PRId32 "\n", hdr->byteRate);
   unblockedMessage("Block align     :%" PRId16 "\n", hdr->blockAlign);
   unblockedMessage("Bits per sample :%" PRId16 "\n", hdr->bitsPerSample);
   unblockedMessage("Subchunk2 ID    :%s\n", hdr->subchunk2ID);
   unblockedMessage("Subchunk2 size  :%" PRId32 "\n", hdr->subchunk2Size);
}/*prnPcmHeader*/

static char *readHeader (wav_hdr_t *hdr, FILE **pcm, char *fileName)
{
   int l = g_pathnameLength + strlen(fileName) + 1;
   /*TODO: malloc may fails*/
   char *fullname = strcpy(malloc(l),g_pathname);
   strcat(fullname, fileName);

   free(fileName);

   *pcm = fopen(fullname, "rb");
   /*TODO: error check*/

   {/*Read header:*/
      int pcmFileSize = getFileSize(*pcm);
      int chunkSize = getPcmHeader(hdr, *pcm, 1);
      if ( (0 > chunkSize) || (8 != (pcmFileSize - chunkSize) ) )
      {
         blockMessage();
         unblockedMessage("**** Ignore file '%s':\n", fullname);
         unblockedMessage(" Unsupported adio format or broken data.\n\n");
         prnPcmHeader (hdr);
         unblockMessage();
         fclose (*pcm);
         *pcm = NULL;
         free (fullname);
         return NULL;
      }
   }
   return fullname;
}/*readHeader*/

/*some cleanup*/
static void cln(int id, char *fileName, FILE *pcm, FILE *mp3, unsigned int *counter)
{
   if (NULL != fileName)
   {
     free(fileName);
   }
   if (NULL != pcm)
   {
      fclose(pcm);
   }
   if (NULL != mp3)
   {
      fclose(mp3);
   }
   if (NULL != counter)
   {
      pthread_mutex_lock(&g_mTotalConverted);
      /*atomic increment would be quite enough, BTW..,*/
      ++(*counter);
      pthread_mutex_unlock(&g_mTotalConverted);
   }
}/*cln*/

/* The main routine performing a real encoding*/
static void theWorker(int id)
{

   for(;;)/*mail loop*/
   {
      wav_hdr_t hdr;
      char *fileName = NULL;
      FILE *pcm = NULL, *mp3 = NULL;
      lame_t gf = NULL;
      size_t numRead = 0, numWrite = 0;

      int16_t pcm_buffer[PCM_SIZE*2];
      unsigned char mp3_buffer[MP3_SIZE];
      int l;

      /*First, wait for a file name to encode:*/
      sem_wait(&g_sFileName);
      /*There was something in the queue, try to catch it:*/
      pthread_mutex_lock(&g_mFileName);
      fileName = txt_qFLPop(&g_qFileName);
      pthread_mutex_unlock(&g_mFileName);
      if (NULL == fileName)
      {
        /*Somebody was faster :( Impossible, BTW! Intrnal error...*/
        continue;
      }

      /*We have got something*/
      if ('\0' == *fileName)
      {
         /*That's all*/
         return;
      }

      /*We have some file name to encode. Convert it to a full-path name,
        open a file and read a header:*/
      fileName = readHeader(&hdr, &pcm, fileName);

      if(NULL == fileName)
      {
         /*wrong format, input file was closed*/
         cln(id, NULL, NULL, NULL, NULL);
         continue;
      }

      /*init lame with parameters compatible with ones read from the header:*/
      gf = lame_init();
      if ( NULL ==   gf )
      {
         errorMsg("File '%s': lame_init fails\n", fileName);
         cln(id, fileName, pcm, NULL, NULL);
         continue;
      }
      lame_set_num_channels(gf, hdr.NumChannels);
      lame_set_quality(gf, 3);
      lame_set_in_samplerate(gf, hdr.sampleRate);
      //lame_set_VBR(gf, vbr_default);/*sometimes segfaults*/
      lame_set_VBR(gf, vbr_rh);
      //lame_set_compression_ratio(gf, 5);

      if ( -1 ==   lame_init_params(gf) )
      {
         errorMsg("File '%s': lame_init fails\n", fileName);
         cln(id, fileName, pcm, NULL, NULL);
         continue;
      }

      /*Now proceed the output file.*/
      /*"...wav" -> ...mp3:*/
      l = strlen(fileName);
      fileName[--l] = '3';
      fileName[--l] = 'p';
      fileName[--l] = 'm';

#ifdef DO_NOT_OVERRIDE
      mp3 = fopen(fileName, "r");
      if (NULL != mp3)
      {
         message("File '%s' exists, do nothing\n", fileName);
         fclose(mp3);
         mp3 = NULL;
         cln(id, fileName, pcm, NULL, NULL);
         continue;
      }
#endif
      /*Can't create file at the scan time:( :*/
      pthread_mutex_lock(&g_mEndScan);      
      mp3 = fopen(fileName, "wb");
      pthread_mutex_unlock(&g_mEndScan);

      /*Encode the file:*/
      do 
      {
         numRead = fread(pcm_buffer, 2*sizeof(int16_t), PCM_SIZE, pcm);
         if (numRead == 0)
         { 
            numWrite = lame_encode_flush(gf, mp3_buffer, MP3_SIZE);
         }
         else
         {
            if (1 == hdr.NumChannels)
            {
               numWrite = lame_encode_buffer(gf, pcm_buffer, pcm_buffer, numRead*2 , mp3_buffer, MP3_SIZE);
            }
            else
            {
               numWrite = lame_encode_buffer_interleaved(gf, pcm_buffer, numRead, mp3_buffer, MP3_SIZE);
            }
            if (numWrite < 0)
            {
               /*TODO: error!*/
            }
         }
         fwrite(mp3_buffer, numWrite, 1, mp3);
      }
      while (numRead != 0);
      /*ready*/

      lame_close(gf);
      gf = NULL;
      cln (id, fileName, pcm, mp3, &g_totalConverted);/*here g_totalConverted was incremented*/
      mp3 = pcm = NULL;

      message("%u files processed\n", g_totalConverted);

   }/*for(;;)*/
}/*theWorker*/

static void *startWorker (void *ptr)
{
   int id;
   {
      pool_t *p = (pool_t*)ptr;
      id = p - g_pool;/*This is the order number of my resources*/
   }
   message(" started worker %d\n", id);

   /*finish setup:*/
   sem_post(&g_sAllThreadsReady);/*indicator end of initialisation*/

   /*do the job*/
   theWorker(id);

   return NULL;
}/*startWorker*/

/*This callback is invoked on each of a scanned directory element*/
static int doScanDirectory( int i, char *dirEntry, int isDirectory, void *data)
{
   int *n = (int*)data;
   if (!isDirectory)
   {
      size_t l = strlen(dirEntry);
      char *ch = dirEntry + (l - 4);
      if (
            ('.' == ch[0]) &&
            ('W' == myToupper(ch[1])) &&
            ('A' == myToupper(ch[2])) &&
            ('V' == myToupper(ch[3]))
         )
      {
         /*TODO: mutex lock/unlock and sem_post may fail:*/
         pthread_mutex_lock(&g_mFileName);
         /*TODO: malloc and txt_qFLPushFifo may fail!*/
         txt_qFLPushFifo(&g_qFileName, strcpy(malloc(l+1),dirEntry));
         sem_post(&g_sFileName);
         pthread_mutex_unlock(&g_mFileName);
         ++(*n);
      }
   }
   return 0;
}/*scanDirectory*/

static int scanDirectory (char *pathName)
{
   int foundItems = 0;
   int i;
   int tot = listDir(pathName, doScanDirectory, (void*)&foundItems);
   if (tot < 0)
   {
      return tot;
   }

   /*send empty file name as end-of-work:*/
   /*TODO: mutex lock/unlock and sem_post may fail:*/
   pthread_mutex_lock(&g_mFileName);
   for (i = 0; i < g_nWorkers; ++i)
   {
      txt_qFLPushFifo(&g_qFileName, "");
      sem_post(&g_sFileName);
   }
   pthread_mutex_unlock(&g_mFileName);
   if (0 == foundItems)
   {
      message("No files found!\n");
   }   
   return foundItems;
}/*scanDirectory*/

int main(int argc, char *argv[])
{
   int i;

   if (2 != argc)
   {
      halt(10, "usage: %s pathname\n", argv[0]);
   }

   /*Save a name of a given directory to scan as ".../
     ( ...or \ on Windows)"*/
   for (i=0; '\0' != argv[1][i]; ++i)
   {
      if (i >= MAX_PATH_LENGTH - 1)
      {
         halt(15, "Too long path name\n");
      }
      g_pathname[i] = argv[1][i];
   }
   g_pathnameLength = i;
   
   if (SYSTEM_DIR_DELIMITER != g_pathname[g_pathnameLength - 1])
   {
      g_pathname[g_pathnameLength++] = SYSTEM_DIR_DELIMITER;
   }

   g_pathname[g_pathnameLength] = '\0';

   g_cpuNumber =  getCpuNumber();

   if (0 == g_nWorkers)
   {
      g_nWorkers = g_cpuNumber + g_cpuNumber / 2;
   }/*else TODO: manually setting instead of hardcoding*/

   if ( 
        (sem_init(&g_sFileName, 0, 0) == -1)||
        (sem_init(&g_sAllThreadsReady, 0, 0) == -1)
      )
   {
      halt(10, "Error initialising global semaphores\n");
   }

   /*We can't change the content of a directory while scanning ):*/
   pthread_mutex_lock(&g_mEndScan);

   /*TODO: qFLInit may fail*/
   /*initialize file names queue:*/
   txt_qFLInit(NULL, 0, &g_qFileName, QFL_REALLOC_IF_FULL);

   g_pool = malloc( g_nWorkers * sizeof(pool_t) );

   if (NULL == g_pool )
   {
       halt(15, "malloc fails\n");
   }

  /*start workers:*/
   for(i = 0; i < g_nWorkers; ++i)
   {
      if(
            pthread_create (g_pool + i,
                         NULL, startWorker,
                        (void*)(g_pool + i))
        )
      {
         halt(20, "Can't start worker %d of %d\n", i, g_nWorkers);
      }
   }/*for(i = 0; i < g_nWorkers; ++i)*/

   message("Waiting init\n");

   /*Wait when initialization finishes*/
   for(i = g_nWorkers; i > 0; --i)
   {
      sem_wait(&g_sAllThreadsReady);
   }

   message("Everybody is ready, start scanner\n");

   i = scanDirectory( argv[1]);
   if ( i < 0 )
   {
      halt(2, "Internal error\n");
   }

   message("%d .wav files found\n", i);
   pthread_mutex_unlock(&g_mEndScan);
   /*now workers are able to write results to hard drive*/

   /*Wait for the job is finished:*/
   for(i = 0; i < g_nWorkers; ++i)
   {
      pthread_join(g_pool[i], NULL);
   }

   message("\n%u files converted\n", g_totalConverted);
   /*TODO: cleanup*/

   /*Well, next step... in next life!*/
   return 0;
}/*main*/
