#include <string>

class CJdAwsConfig
{
public:
	CJdAwsConfig(const char *pszCfgFile);
	int GetField(char *pszData, const char *pszField, std::string &strVal);
	std::string m_M3u8File;
	std::string m_Folder;
	std::string m_Bucket;
	std::string m_Host;
	std::string m_SecKey;
	std::string m_AccessId;
};