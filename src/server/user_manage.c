#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <mysql.h>
#include <errno.h>

#include "user_manage.h"
#include "utils.h"
#include "log.h"

#define STRMAX 51

/* user info structure */
typedef struct {
    int uid;
    char name[STRMAX];
    char passwd[STRMAX];
} user_info_t;

typedef struct {
    char hostname[51];
    char username[51];
    char password[51];
    char database[51];
    int port;
} mysql_config_t;

mysql_config_t mysql_config =
{
    "localhost",
    "root",
    "root",
    "",
    3306
};


MYSQL * mysql_conn;

int uid = 99;

void mysql_set_connect()
{
    if (mysql_library_init(0, NULL, NULL))
    {
        log_error("count not initialize MySQL client library");
        exit(EXIT_FAILURE);
    }

    mysql_conn = mysql_init(NULL);

    if (NULL == mysql_conn)
    {
        log_error("cloud not create database connection: %s", mysql_error(mysql_conn));
        exit(EXIT_FAILURE);
    }

    log_info("mysql_connect");
    if (!mysql_real_connect(mysql_conn, NULL, "root", "root", NULL, 0, NULL, 0))
    {
        log_error("cloud not connect database: %s", mysql_error(mysql_conn));
        exit(EXIT_FAILURE);
    }

}

void mysql_close_connect()
{
    mysql_close(mysql_conn);
    mysql_library_end();
}

void mysql_create_db_table()
{
    log_info("create database if not exists");
    /* printf("rest: %d\n", mysql_query(mysql_conn, "CREATE DATABASE IF NOT EXISTS chatroom")); */
    if (mysql_query(mysql_conn, "CREATE DATABASE IF NOT EXISTS chatroom") != 0)
    {
        log_error("%s", mysql_error(mysql_conn));
        exit(EXIT_FAILURE);
    }

    log_info("select database chatroom");
    if (mysql_select_db(mysql_conn, "chatroom") != 0)
    {
        log_error("%s", mysql_error(mysql_conn));
        exit(EXIT_FAILURE);
    }

    log_info("create table if not exists");
    const char * stmt = "CREATE TABLE IF NOT EXISTS register_user \
                         ( uid INT(8) NOT NULL UNIQUE AUTO_INCREMENT, \
                           name VARCHAR(51) NOT NULL DEFAULT \"annoymous\", \
                           password VARCHAR(51) NOT NULL, \
                           PRIMARY KEY (uid) \
                         ) ENGINE=InnoDB, AUTO_INCREMENT = 100";
    if (mysql_query(mysql_conn, stmt) != 0)
    {
        log_error("%s", mysql_error(mysql_conn));
    }
}

/* MySQL uid initialization */
void mysql_uid_init()
{
    char stmt[512];
    MYSQL_RES * result;
    MYSQL_ROW row;

    sprintf(stmt, "SELECT uid FROM register_user \
            ORDER BY uid DESC \
            LIMIT 1");
    if (mysql_query(mysql_conn, stmt) != 0)
    {
        log_error("mysql query failed: %s", mysql_error(mysql_conn));
        exit(EXIT_FAILURE);
    }

    result = mysql_store_result(mysql_conn);
    if (result == NULL)
    {
        log_error("mysql storing result failed: %s", mysql_error(mysql_conn));
        exit(EXIT_FAILURE);
    }

    row = mysql_fetch_row(result);
    if (row == NULL)
    {
        uid = uid + 1;
    }
    else
    {
        uid = atoi(row[0]) + 1;
    }

    /* log_debug("mysql uid init: %d", uid); */
}

void mysql_insert_user(int uid, const char * pwd, const char * name)
{
    char stmt[512];
    if (NULL == name || 0 == strlen(name))
    {
        sprintf(stmt, "INSERT INTO register_user \
                (uid, password) VALUES (%d, '%s')", uid, pwd);
    }
    else
    {
        sprintf(stmt, "INSERT INTO register_user \
                (uid, name, password) VALUES (%d, '%s', '%s')", uid, name, pwd);
    }
    printf("stmt: %s\n", stmt);
    if (mysql_query(mysql_conn, stmt) != 0)
    {
        log_error("new user register failed: %s", mysql_error(mysql_conn));
    }

    /* uid = mysql_insert_id(mysql_conn); */
}

int mysql_verify_uid_pwd(const int uid, const char * pwd, char * name)
{
    char stmt[512];
    MYSQL_RES * result;
    MYSQL_ROW row;

    sprintf(stmt, "SELECT password, name FROM register_user \
            WHERE uid = %d", uid);

    if (mysql_query(mysql_conn, stmt) != 0)
    {
        log_error("mysql query failed: %s", mysql_error(mysql_conn));
        return -2;
    }

    result = mysql_store_result(mysql_conn);
    if (result == NULL)
    {
        log_error("mysql storing result failed: %s", mysql_error(mysql_conn));
        return -2;
    }

    row = mysql_fetch_row(result);
    if (row == NULL)
        return -1;
    if (!strcmp(row[0], pwd))
    {
        strcpy(name, row[1]);
        return 0;
    }
    else
        return 1;
}

void mysql_update_pwd_name(const int uid, const char * pwd, const char * name)
{
    char stmt[512];
    if (pwd && name)
    {
        sprintf(stmt, "UPDATE register_user \
                SET password = '%s', name = '%s' \
                WHERE uid = %d", pwd, name, uid);
    }
    else if (pwd)
    {
        sprintf(stmt, "UPDATE register_user \
                SET password = '%s' \
                WHERE uid = %d", pwd, uid);
    }
    else if (name)
    {
        sprintf(stmt, "UPDATE register_user \
                SET name = '%s' \
                WHERE uid = %d", name, uid);
    }

    if (mysql_query(mysql_conn, stmt) != 0)
    {
        log_error("mysql query failed: %s", mysql_error(mysql_conn));
    }
}

static char dbpath[256];

/* uid initialization */
void uid_init()
{
    getprogpath(dbpath, sizeof(dbpath));
    strcat(dbpath, "data/users.db");
    FILE * fp = fopen(dbpath, "a+b");
    if (NULL == fp)
    {
        log_error("users database open failed: %s", strerror(errno));
        exit(EXIT_FAILURE);
    }
    user_info_t user_info;

    while (fread(&user_info, sizeof(user_info_t), 1, fp)) /* stop after an EOF or a newline */
    {
        /* printf("saved user: %d %s %s\n", user_info.uid, user_info.passwd, user_info.name); */
        if (user_info.uid > uid)
            uid = user_info.uid;
    }

    uid = uid + 1;
    /* log_debug("init uid: %d", uid); */
    fclose(fp);
}

/* verify uid and password */
/* 0: login successfully 1: uid and password not match; */
/* -1: uid not found */
int verify_uid_pwd(const int uid, const char * pwd, char * name)
{
    user_info_t user_info;

    FILE * fp = fopen(dbpath, "rb");
    if (NULL == fp)
    {
        log_error("users database open failed: %s", strerror(errno));
        exit(EXIT_FAILURE);
    }

    while (fread(&user_info, sizeof(user_info_t), 1, fp)) /* stop after an EOF or a newline */
    {
        /* printf("saved user: %d %s %s\n", user_info.uid, user_info.passwd, user_info.name); */
        if (user_info.uid== uid)
        {
            if (!strcmp(user_info.passwd, pwd))
            {
                strcpy(name, user_info.name);
                fclose(fp);
                return 0; /* login successfully */
            }
            else
            {
                fclose(fp);
                return 1; /* uid and pwd not match */
            }
        }
    }
    fclose(fp);

    return -1; /* uid not found*/
}

void save_uid_pwd(int uid, const char * pwd, const char * name)
{
    user_info_t user_info;
    user_info.uid = uid;
    strcpy(user_info.passwd, pwd);
    if (name)
    {
        strcpy(user_info.name, name);
    }
    else
    {
        strcpy(user_info.name, "annoymous");
    }
    FILE * fp = fopen(dbpath, "ab");
    if (NULL == fp)
    {
        log_error("users database open failed: %s", strerror(errno));
        exit(EXIT_FAILURE);
    }

    /* printf("saved user: %d %s %s\n", user_info.uid, user_info.passwd, user_info.name); */
    if (fwrite(&user_info, sizeof(user_info_t), 1, fp) != 1)
    {
        log_error("save user failed");
        ferror(fp);
    }
    fclose(fp);
}

void modify_pwd_name(const int uid, const char * pwd, const char * name)
{
    user_info_t user_info;

    FILE * fp = fopen(dbpath, "r+b");
    if (NULL == fp)
    {
        log_error("users database open failed: %s", strerror(errno));
        exit(EXIT_FAILURE);
    }

    while (fread(&user_info, sizeof(user_info_t), 1, fp)) /* stop after an EOF or a newline */
    {
        /* printf("saved user: %d %s %s\n", user_info.uid, user_info.passwd, user_info.name); */
        if (user_info.uid == uid)
            break;
    }
    fseek(fp, -sizeof(user_info_t), SEEK_CUR);
    if (pwd)
    {
        strcpy(user_info.passwd, pwd);
    }
    if (name)
    {
        strcpy(user_info.name, name);
    }
    fwrite(&user_info, sizeof(user_info_t), 1, fp);
    fseek(fp, 0, SEEK_CUR);
    fclose(fp);
}
