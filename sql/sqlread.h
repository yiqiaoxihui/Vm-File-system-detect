#ifndef CSQL_H_INCLUDED
#define CSQL_H_INCLUDED

#include <mysql/mysql.h>

int read_host_image_name(char **image_abspath,char **image_id);


int sql_update_file_metadata(char *overlay_id,__U64_TYPE inode_number,__U16_TYPE mode,__U32_TYPE dtime,int inodePosition,int dataPosition);
#endif // CSQL_H_INCLUDED
