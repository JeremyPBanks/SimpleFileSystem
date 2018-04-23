#include <stdlib.h>
#include <stdio.h>
#include <string.h>


int main(int argc, char *argv[])
{
	int stat;
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

	char cmd[24];
	strcpy(cmd, "./unmount");

	if(saveFiles)
	{strcat(cmd, "-s");}

	if(saveFlat)
	{strcat(cmd, " t");}

	if(saveMount)
	{strcat(cmd, " m");}

	if((stat = system(cmd))	< 0)
	{
		printf("Failed to unmount\n");
		return stat;
	}

	if((stat = system("./mount")) < 0)
	{
		printf("Failed to mount\n");
		return stat;
	}

	return 0;
}
