#ifndef TPG_APP_HH
#define TPG_APP_HH

namespace TPGen {
  class TPG;	
  int execute(int argc, char* argv[], TPGen::TPG*, bool lAsync=true);
  const char* usage();
  const char* opts();
};

#endif
