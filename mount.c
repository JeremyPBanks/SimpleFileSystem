#include <stdlib.h>
#include <stdio.h>


int main(int argc, char *argv[])
{
	int stat;
	char *myFolder = "cd /.freespace && mkdir laf224";
	char *flat = "cd /.freespace/laf224 && touch testfsfile";
	char *tmp = "cd /tmp && mkdir laf224 && cd laf224 && mkdir mountdir";
	char *mount = "cd /ilab/users/laf224/Documents/CS416/Project3/src && ./sfs /.freespace/laf224/testfsfile /tmp/laf224/mountdir";

	if((stat = system(myFolder)) < 0)
	{
		printf("Failed on %s\n", myFolder);
		return stat;
	}

	if((stat = system(flat)) < 0)
	{
		printf("Failed on %s\n", flat);
		return stat;
	}
	
	if((stat = system(tmp)) < 0)
	{
		printf("Failed on %s\n", tmp);
		return stat;
	}

	if((stat = system(mount)) < 0)
	{
		printf("Failed on %s\n", mount);
		return stat;
	}

	return stat;
}
