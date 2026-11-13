#include "mapquality.h"

namespace cartographer::mapquality {

int mapQuality(int rolled, bool isMap)
{
	return isMap && rolled == Magic ? Normal : rolled;
}

}  // namespace cartographer::mapquality
