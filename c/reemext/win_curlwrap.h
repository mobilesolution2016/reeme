extern "C" {
#include "curl.h"

	typedef CURL *(*fn_curl_easy_init)(void);
	typedef CURLcode(*fn_curl_easy_setopt)(CURL *curl, CURLoption option, ...);
	typedef CURLcode(*fn_curl_easy_perform)(CURL *curl);
	typedef void(*fn_curl_easy_cleanup)(CURL *curl);
	typedef CURLcode (*fn_curl_easy_getinfo)(CURL *curl, CURLINFO info, ...);
}

fn_curl_easy_init p_curl_easy_init;
fn_curl_easy_setopt p_curl_easy_setopt;
fn_curl_easy_perform p_curl_easy_perform;
fn_curl_easy_cleanup p_curl_easy_cleanup;
fn_curl_easy_getinfo p_curl_easy_getinfo;

size_t curl_write_tostring(void *ptr, size_t size, size_t nmemb, void *userp)
{
	std::string* pstr = (std::string*)userp;
	pstr->append((char*)ptr, size * nmemb);

	return size * nmemb;
}

size_t curl_write_tofile(void *ptr, size_t size, size_t nmemb, void *filep)
{
	FILE* f = (FILE*)filep;
	fwrite((char*)ptr, 1, size * nmemb, f);

	return size * nmemb;
}

CURL* curlInitOne(const char* url, const char* posts = 0, size_t postsSize = 0)
{
	const char* protoEnd = strchr(url, ':');
	if (!protoEnd)
		return 0;

	if (protoEnd - url > 5)
		return 0;

	bool useSSL;
	char proto[8] = { 0 };
	memcpy(proto, url, protoEnd - url);
	strlwr(proto);

	if (strcmp(proto, "http") == 0)
		useSSL = false;
	else if (strcmp(proto, "https") == 0)
		useSSL = true;
	else
		return 0;

	CURL* curl = p_curl_easy_init();
	if (!curl)
		return 0;

	p_curl_easy_setopt(curl, CURLOPT_URL, url);
	p_curl_easy_setopt(curl, CURLOPT_NOSIGNAL, 1);
	if (posts && posts[0])
	{
		p_curl_easy_setopt(curl, CURLOPT_POST, 1);
		p_curl_easy_setopt(curl, CURLOPT_POSTFIELDS, posts);
		p_curl_easy_setopt(curl, CURLOPT_POSTFIELDSIZE, postsSize);
	}
	if (useSSL)
	{
		p_curl_easy_setopt(curl, CURLOPT_SSL_VERIFYPEER, FALSE);
		p_curl_easy_setopt(curl, CURLOPT_SSL_VERIFYHOST, FALSE);
	}

	return curl;
}

CURLcode curlExecOne(CURL* curl, std::string& strout)
{
	p_curl_easy_setopt(curl, CURLOPT_WRITEDATA, &strout);
	p_curl_easy_setopt(curl, CURLOPT_WRITEFUNCTION, &curl_write_tostring);
	CURLcode res = p_curl_easy_perform(curl);
	p_curl_easy_cleanup(curl);

	return res;
}

CURLcode curlExecOne(CURL* curl, FILE* fp)
{
	p_curl_easy_setopt(curl, CURLOPT_WRITEDATA, fp);
	p_curl_easy_setopt(curl, CURLOPT_WRITEFUNCTION, &curl_write_tofile);
	CURLcode res = p_curl_easy_perform(curl);
	p_curl_easy_cleanup(curl);

	return res;
}