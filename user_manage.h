#ifndef USER_MANAGE_H_
#define USER_MANAGE_H_

extern int uid;

void mysql_set_connect();
void mysql_close_connect();
void mysql_create_db_table();
void mysql_uid_init();
void mysql_insert_user(int uid, const char * pwd, const char * name);
int mysql_verify_uid_pwd(const int uid, const char * pwd, char * name);
void mysql_update_pwd_name(const int uid, const char * pwd, const char * name);

void uid_init();
int verify_uid_pwd(const int uid, const char * pwd, char * name);
void save_uid_pwd(int uid, const char * pwd, const char * name);
void modify_pwd_name(const int uid, const char * pwd, const char * name);
#endif
