#ifndef TPG_DEVICE_CPSW_HH
#define TPG_DEVICE_CPSW_HH

#include <cpsw_api_builder.h>

class   ITPG;
typedef shared_ptr<ITPG> TPG;

class ITPG : public virtual IMMIODev {
public:
	static TPG create(const char *name);
};

#endif
