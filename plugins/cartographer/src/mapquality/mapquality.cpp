#include "mapquality.h"

namespace cartographer::mapquality {

int mapQuality(int rolled, bool isMap)
{
	return isMap && rolled == Magic ? Normal : rolled;
}

bool useNormalFallback(bool isMap)
{
	return isMap;
}

}  // namespace cartographer::mapquality
