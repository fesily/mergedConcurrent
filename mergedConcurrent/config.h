#pragma once

#ifndef MERGED_TARGET
	#if defined WIN32 && defined _MSC_VER
		#define MERGED_TARGET_PPL 1
		#define MERGED_TARGET MERGED_TARGET_PPL
	#endif
#endif


#ifndef MERGED_TARGET
#error "not support this c++ compiler"
#endif
