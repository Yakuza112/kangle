/*
 * Copyright (c) 2010, NanChang BangTeng Inc
 * All Rights Reserved.
 *
 * You may use the Software for free for non-commercial use
 * under the License Restrictions.
 *
 * You may modify the source code(if being provieded) or interface
 * of the Software under the License Restrictions.
 *
 * You may use the Software for commercial use after purchasing the
 * commercial license.Moreover, according to the license you purchased
 * you may get specified term, manner and content of technical
 * support from NanChang BangTeng Inc
 *
 * See COPYING file for detail.
 */
#include "KFileName.h"
#include "utils.h"
#include <vector>
#include <iostream>
#include "malloc_debug.h"
#include "forwin32.h"
#include "KVirtualHost.h"
#ifdef _WIN32
#include <direct.h>
#endif
#ifdef _WIN32
/*
文件名转unicode,并猜测编码
*/
wchar_t *FileNametoUnicode(const char *str,int len)
{
	wchar_t *s = (wchar_t *)xmalloc(2*(len+1));
	int len2 = MultiByteToWideChar(CP_UTF8,MB_ERR_INVALID_CHARS,str,len,(LPWSTR)s,len);
	if(len2==0){
		len2 = MultiByteToWideChar(CP_ACP,MB_ERR_INVALID_CHARS,str,len,(LPWSTR)s,len);
	}
	s[len2] = L'\0';
	//wprintf(L"s=[%s],len=[%d]\n",s,len2);
	return s;
}
wchar_t *toUnicode(const char *str,int len,int cp_code)
{
	if(len==0){
		len=strlen(str);
	}
	wchar_t *s = (wchar_t *)xmalloc(2*(len+1));
	len = MultiByteToWideChar(cp_code,0,str,len,(LPWSTR)s,len);
	s[len] = L'\0';
	return s;
}
#endif
using namespace std;
KFileName::KFileName() {
	name = NULL;
	index = NULL;
	prev_dir = false;
#ifdef _WIN32
	wname = NULL;
#endif
	linkChecked = false;
	pathInfoLength = 0;
	ext = NULL;
}
KFileName::~KFileName() {
	if (name) {
		assert(name_len == strlen(name));
		xfree(name);
	}
#ifdef _WIN32
	if(wname){
		xfree(wname);
	}
#endif
	if (index) {
		xfree(index);
	}
	if(ext){
		xfree(ext);
	}
}
bool KFileName::operator ==(KFileName &a) {
#if 0
	if(a.name == this->name == NULL) {
		return true;
	}
	if(a.name == NULL || this->name == NULL) {
		return false;
	}
#endif
	return filecmp(a.name, this->name) == 0;
}
char *KFileName::makeExt(const char *name)
{
	const char *p = strrchr(name, '.');
	if (p == NULL){
		return NULL;
	}
	p++;
	if (strchr(p, '/') != NULL){
		return NULL;
	}
	return xstrdup(p);
}
const char *KFileName::getExt() {
	if(ext){
		return ext;
	}
	ext = makeExt(name);
	return ext;
}
char *KFileName::concatDir(const char *docRoot, const char *file) {
	char *triped_path = tripDir2(file, '/');
	if (triped_path == NULL) {
		return NULL;
	}
	int doclen = strlen(docRoot);
	int len = strlen(triped_path);
	int name_len = doclen + len;
	char *name = (char *) xmalloc(name_len+2);
	memcpy(name, docRoot, doclen);
	if (docRoot[doclen - 1] != '/'
#ifdef _WIN32
		&& docRoot[doclen-1] != '\\'
#endif
		) {
		name[doclen] = PATH_SPLIT_CHAR;
		doclen++;
		name_len++;
	}
	if (*triped_path == '/'
#ifdef _WIN32
		|| *triped_path == '\\'
#endif
		) {
		memcpy(name + doclen, triped_path + 1, len - 1);
		name_len--;
	} else {
		memcpy(name + doclen, triped_path, len);
	}
	name[name_len] = '\0';
	xfree(triped_path);
	return name;
}
void KFileName::tripDir3(char *path,const char split_char)
{
	char *src = path;
	char *hot = path;
	for (;;) {
		if (*hot == '\0') {
			*src = '\0';
			return;
		}
		if (*hot == '/' || *hot == '\\') {
			*src = split_char;
			src++;
			//跳到不是分格符的位置
			for (;;) {
				if (!*hot) {
					*src = '\0';
					return;
				}
				if (*hot == '/' || *hot == '\\') {
					hot++;
				} else {
					break;
				}
			}
		}
		//不是分格符
		char *p = hot;
		//找下面的分格符
		while (*p && *p != '/' && *p != '\\')
			p++;
		//找到了
		int copy_len = p - hot;
		if (*hot == '.') {
			if (copy_len == 1) {
				//如果是本目录，则略过
				if (src > path) {
					src--;
					assert(*src==split_char);
				}
				hot = p;
				continue;
			}
			if (copy_len == 2 && *(hot + 1) == '.') {
				//如果是上级目录

				if (src > path) {
					src--;
					assert(*src==split_char);
				}
				if (src > path) {
					src--;
					while (src > path && *src != split_char) {
						src--;
					}
				}
				hot = p;
				continue;
			}
		}
		if (src != hot) {
			memcpy(src, hot, p - hot);
			src += copy_len;
		} else {
			src = p;
		}
		hot = p;
	}
	//不可能到这里来的。
	assert(false);
}
char *KFileName::tripDir2(const char *dir, const char split_char) {
	if (dir == NULL) {
		return NULL;
	}
	char *path = (char *) xstrdup(dir);
	tripDir3(path,split_char);
	return path;
}
bool KFileName::tripDir(std::string &dir) {
	size_t i;
	char *ptr;
	char *split_str = strdup(dir.c_str());
	char *tmp = split_str;
	char *msg;
	vector<string> dirs;
	vector<string> real_dir;
	for (i = 0; i < strlen(split_str); i++) {
		if (split_str[i] == '\\')
			split_str[i] = '/';
	}
	while ((msg = my_strtok(tmp, '/', &ptr)) != NULL) {
		tmp = NULL;
		dirs.push_back(msg);
	}
	//	printf("dirs size=%d\n",dirs.size());
	free(split_str);
	dir = "";
	for (i = 0; i < dirs.size(); i++) {
		if (dirs[i] == "")
			continue;
		if (dirs[i] == ".")
			continue;
		if (dirs[i] == "..") {
			if (real_dir.size() <= 0) {
				return false;
			}
			real_dir.pop_back();
			continue;
		}
		real_dir.push_back(dirs[i]);
	}
	for (i = 0; i < real_dir.size(); i++) {
		if (i > 0) {
			dir = dir + PATH_SPLIT_CHAR;
		}
		dir += real_dir[i];

	}
	return true;
}
bool KFileName::isDirectory() {
	if(prev_dir){
		return true;
	}
	return S_ISDIR(buf.st_mode) > 0;
}
bool KFileName::canExecute() {
#ifndef _WIN32
	return TEST(buf.st_mode,S_IXOTH);
#else
	return true;
#endif
}
void KFileName::setIndex(const char *index) {
	if (this->index) {
		xfree(this->index);
	}
	this->index = strdup(index);
}
time_t KFileName::getLastModified() const{
	return buf.st_mtime;
}
bool KFileName::getFileInfo() {
	if (name == NULL || name_len<1) {
		return false;
	}
	char *c = name+name_len-1;

	if(*c=='/' || *c== '\\') {
		prev_dir = true;	
		/*
		在windows下，一个家目录的上级目录，该虚拟主机运行身份没有权限，则导致stat失败。
		如果有默认文档时，则不能正确获得。
		如果这种情况下，不应该往下stat。
		*/
		return true;
	}
#ifdef _WIN32
	if(getNameW()==NULL){
		return false;
	}
	if(_wstati64(wname,&buf) != 0){
		return false;
	}
#else
	if (_stati64(name, &buf) != 0) {
		//fprintf(stderr, "cann't stat file %s errno=%d %s\n",m_name.c_str(),errno,strerror(errno));
		return false;
	}
#endif
	fileSize = buf.st_size;
	return true;

}
const char *KFileName::getName() {
	return name;
}
size_t KFileName::getNameLen() {
	return name_len;
}
bool KFileName::giveName(char *path) {
	if (name) {
		xfree(name);
	}
	name = path;
	name_len = strlen(path);
#ifdef _WIN32
	if(wname){
		xfree(wname);
		wname = NULL;
	}
#endif
	return getFileInfo();
}
bool KFileName::setName(const char *path) {
	if (name) {
		xfree(name);
	}
	name = xstrdup(path);
	name_len = strlen(path);
#ifdef _WIN32
	if(wname){
		xfree(wname);
		wname = NULL;
	}
#endif
	return getFileInfo();
}
/*

*/
CheckLinkState KFileName::checkLink(const char *path, int follow_link) {
#ifdef _WIN32
	if(wname){
		xfree(wname);
	}
	wname = FileNametoUnicode(path,strlen(path));
	if(wname==NULL || _wstati64(wname,&buf) != 0){
		return CheckLinkFailed;
	}
#else
	if (lstat(path, &buf) != 0) {
		//fprintf(stderr, "cann't stat file %s errno=%d %s\n",m_name.c_str(),errno,strerror(errno));
		return CheckLinkFailed;
	}
	if (S_ISLNK(buf.st_mode)) {
		//link file
		struct stat buf2;
		if (stat(path, &buf2) != 0) {
			return CheckLinkFailed;
		}
		if (TEST(follow_link,FOLLOW_LINK_OWN)) {
			if (buf.st_uid != buf2.st_uid) {
				return CheckLinkFailed;
			}
		}
		memcpy(&buf, &buf2, sizeof(buf));
		if (TEST(follow_link,FOLLOW_LINK_OWN|FOLLOW_LINK_ALL)) {
			return CheckLinkContinue;
		}
		return CheckLinkFailed;
	}
#endif
	if(TEST(follow_link,FOLLOW_PATH_INFO) && S_ISREG(buf.st_mode)){
		return CheckLinkIsFile;
	}
	return CheckLinkContinue;
}
bool KFileName::setName(const char *docRoot, const char *triped_path,
		int follow_link) {
	if (name) {
		xfree(name);
		name = NULL;
	}
#ifdef _WIN32
	if(wname){
		xfree(wname);
		wname = NULL;
	}
#endif
	size_t doclen = strlen(docRoot);
	if (doclen <= 0) {
		return false;
	}	
	if (TEST(follow_link,FOLLOW_LINK_ALL)) {
		assert(triped_path);
		if (triped_path == NULL) {
			return false;
		}
		int len = strlen(triped_path);
		name_len = doclen + len;
		name = (char *) xmalloc(name_len+2);
		memcpy(name, docRoot, doclen);
		if (docRoot[doclen - 1] != '/' 
#ifdef _WIN32
			&& docRoot[doclen - 1] != '\\'
#endif
			) {
			name[doclen] = PATH_SPLIT_CHAR;
			doclen++;
			name_len++;
		}
		if (*triped_path == '/'
#ifdef _WIN32
			|| *triped_path == '\\'
#endif
			) {
			memcpy(name + doclen, triped_path + 1, len - 1);
			name_len--;
		} else {
			memcpy(name + doclen, triped_path, len);
		}
		name[name_len] = '\0';
		return getFileInfo();
	}
	linkChecked = true;
	int path_len = doclen + strlen(triped_path) + 1;
	char *path = (char *) xmalloc(path_len+1);
	memcpy(path, docRoot, doclen);
	char *dst = path + doclen;
	if (path[doclen - 1] == '/' 
#ifdef _WIN32
		|| path[doclen - 1] == '\\'
#endif
		) {
		dst--;
	}
	const char *src = triped_path;
	const char *ended = triped_path + strlen(triped_path);
	if (*src == '/') {
		src++;
	}
	CheckLinkState result = CheckLinkContinue;
	for (;;) {
		const char *p = strchr(src, '/');
		if (p == NULL) {
			int copy_len = ended - src;
			assert(copy_len>=0);
			if (copy_len <= 0) {
				*dst = '\0';
				prev_dir = true;
				if (result != CheckLinkContinue) {
					result = checkLink(path, follow_link);
					break;
				}
				break;
			}
			*dst = PATH_SPLIT_CHAR ;
			dst++;
			memcpy(dst, src, copy_len);
			dst[copy_len] = '\0';
			result = checkLink(path, follow_link);
			break;
		}
		int copy_len = p - src;
		*dst = PATH_SPLIT_CHAR;
		dst++;
		memcpy(dst, src, copy_len);
		dst += copy_len;
		*dst = '\0';
		result = checkLink(path, follow_link);
		if (result != CheckLinkContinue) {
			break;
		}
		src = p + 1;
	}
	name = path;
	name_len = strlen(path);
	if(result==CheckLinkIsFile){
		const char *p = strchr(src,'/');
		if(p){
			pathInfoLength = p - triped_path;
		}
	}
	if (result == CheckLinkContinue || result == CheckLinkIsFile) {
		fileSize = buf.st_size;
		return true;
	}
	assert(ext==NULL);
	ext = makeExt(triped_path);
	return false;
}
char *KFileName::saveName() {
	char *n = name;
	name = NULL;
#ifdef _WIN32
	if(wname){
		xfree(wname);
		wname = NULL;
	}
#endif
	return n;
}
void KFileName::restoreName(char *n) {
	if (name) {
		xfree(name);
	}
	name = n;
	name_len = strlen(name);
#ifdef _WIN32
	if(wname){
		xfree(wname);
		wname = NULL;
	}
#endif
}
#ifdef _WIN32
const wchar_t *KFileName::getNameW()
{
	if(wname){
		return wname;
	}
	if(name==NULL){
		return NULL;
	}
	wname = FileNametoUnicode(name,name_len);
	return wname;
}
#endif
