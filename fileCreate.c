#include <stdlib.h>
#include <stdio.h>


int main(int argc, char *argv[])
{	
	int i;
	char *cmd;

	for(i = 0; i < 129; i++)
	{
		asprintf(&cmd, "cd /tmp/laf224/mountdir && touch %s%d.txt", "HeyThisIsAReallyInterestingFileNameICameUpWith", i);
		system(cmd);
		free(cmd);
	}


	return 0;
}
