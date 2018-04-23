#include <stdlib.h>
#include <stdio.h>
#include <string.h>


int main(int argc, char *argv[])
{
	int i;
	int saveFiles, saveFlat = 0, saveMount = 0; //if flag set, don't delete, only unmount

	for(i = 1; i < argc; i++)
	{
		if(strcmp(argv[i], "-s") == 0)
		{
			saveFiles = 1;
			continue;
		}

		if(saveFiles && argv[i][0] == 'm')
		{saveMount = 1;}

		if(saveFiles && argv[i][0] == 't')
		{saveFlat = 1;}
	}

	int stat;
	char *unmount = "cd / && fusermount -u /tmp/laf224/mountdir";
	char *flat = "cd /.freespace/laf224 && rm testfsfile";
	char *tmp = "cd /tmp && rm -rf laf224";

	if((stat = system(unmount)) < 0)
	{
		printf("Failed on %s\n", unmount);
		return stat;
	}

	if(!saveFlat)
	{
		if((stat = system(flat)) < 0)
		{
			printf("Failed on %s\n", flat);
			return stat;
		}
	}
	
	if(!saveMount)
	{
		if((stat = system(tmp)) < 0)
		{
			printf("Failed on %s\n", tmp);
			return stat;
		}
	}

	return stat;
}
