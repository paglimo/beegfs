#include <tests/TestRunnerBase.h>

#include <common/app/AbstractApp.h>

#include <cstring>
#include <iostream>

__attribute__((noreturn))
static void usage()
{
   std::cerr << "Usage: test-runner <--xml|--text|--compiler> [output-file]" << std::endl;
   exit(1);
}

int main(int argc, char* argv[])
{
   if(argc != 2 && argc != 3)
      usage();

   TestRunnerOutputFormat format;

   if(strcmp("--xml", argv[1]) == 0)
      format = TestRunnerOutputFormat_XML;
   else
   if(strcmp("--text", argv[1]) == 0)
      format = TestRunnerOutputFormat_TEXT;
   else
   if(strcmp("--compiler", argv[1]) == 0)
      format = TestRunnerOutputFormat_COMPILER;
   else
      usage();

   const char* outFile = "";
   if(argc == 3)
      outFile = argv[2];

   AbstractApp::runTimeInitsAndChecks();

   Logger::createLogger(0, LogType_LOGFILE, true, true, "", "", 0, 0);

   try
   {
      TestRunnerBase runner(outFile, format);

      return runner.run() ? 0 : 1;
   }
   catch(const ComponentInitException& e)
   {
      std::cerr << e.std::exception::what() << std::endl;
      return 1;
   }
}
