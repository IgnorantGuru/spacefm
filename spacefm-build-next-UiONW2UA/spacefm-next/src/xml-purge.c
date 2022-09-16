#include <stdio.h>
#include <string.h>
#include <stdlib.h>

#include <sys/types.h>
#include <sys/stat.h>
#include <fcntl.h>
#include <unistd.h>

#define IS_BLANK(ch)	strchr(" \t\n\r", ch)

static void purge_file( const char* file )
{
	struct stat statbuf;
	int fd;
	char* buf, *pbuf;
	int in_tag = 0, in_quote = 0;
	FILE* fo;

	fd = open( file, O_RDONLY );
	if( fd == -1 )
		return;

	if( fstat( fd, &statbuf) == -1 )
		return;

	if( buf = (char*)malloc( statbuf.st_size + 1 ) )
	{
		if( read( fd, buf, statbuf.st_size) == -1 )
		{
			free( buf );
			return;
		}
		buf[ statbuf.st_size ] = '\0';
	}
	close( fd );

	fo = fopen( file, "w" );
	if( ! fo )
		goto error;

	for( pbuf = buf; *pbuf; ++pbuf )
	{
		if( in_tag > 0 )
		{
			if( in_quote )
			{
				if( *pbuf == '\"' )
					in_quote = 0;
			}
			else
			{
				if( *pbuf == '\"' )
					++in_quote;
				if( ! in_quote && IS_BLANK(*pbuf) )	/* skip unnecessary blanks */
				{
					do{
						++pbuf;
					}while( IS_BLANK( *pbuf ) );

					if( *pbuf != '>' )
						fputc( ' ', fo );
					--pbuf;
					continue;
				}
			}
			if( *pbuf == '>' )
				--in_tag;
			fputc( *pbuf, fo );
		}
		else
		{
			if( *pbuf == '<' )
			{
				if( 0 == strncmp( pbuf, "<!--", 4 ) )	/* skip comments */
				{
					pbuf = strstr( pbuf, "-->" );
					if( ! pbuf )
						goto error;
					pbuf += 2;
					continue;
				}
				++in_tag;
				fputc( '<', fo );
			}
			else
			{
				char* tmp = pbuf;
				while( *tmp && IS_BLANK( *tmp ) && *tmp != '<' )
					++tmp;
				if( *tmp == '<' )	/* all cdata are blank characters */
					pbuf = tmp - 1;
				else /* not blank, keep the cdata */
				{
					if( tmp == pbuf )
						fputc( *pbuf, fo );
					else
					{
						fwrite( pbuf, 1, tmp - pbuf, fo );
						pbuf = tmp - 1;
					}
				}
			}
		}
	}
	
	fclose( fo );

error:
	free( buf );
}

int main( int argc, char** argv )
{
	int i;
	if( argc < 2 )
		return 1;

	for( i = 1; i < argc; ++i )
		purge_file( argv[ i] );

	return 0;
}
