/*  This program is free software: you can redistribute it and/or modify
    it under the terms of the GNU General Public License as published by
    the Free Software Foundation, either version 3 of the License, or
    (at your option) any later version.
    This program is distributed in the hope that it will be useful,
    but WITHOUT ANY WARRANTY; without even the implied warranty of
    MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
    GNU General Public License for more details.
    For more details, see <http://www.gnu.org/licenses/>. */


#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <string.h>
#include <poll.h>
#include <sys/time.h>
#include <sys/types.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <netdb.h>


//struct to store config options
struct config
{
  char username[31];
	char password[31];
	char server[31];
	char fileConf[31];
	int port;
	int requirelog; //not implemented
	int maxconns;
} gconf;
//global variables
const int DEBUG=0;

//function prototypes
int configRead();
int nntpLine(int fd, char *cmd, int show);
int nntpData(int fd, char *cmd);
int nntpLogin(int fd);
int nntpQuit(int fd);


/* Todo:
 * - remove bcopy for s_addr.s_in
 * - output list via command line option (1860573 bytes)
 * - select a group
 * - retrieve a billion headers
 * - stores the headers in a searchable file
 * 
 */


void error(const char *msg)
{
    perror(msg);
    exit(0);
}


//main mostly processes the command args and runs through a control structure
int main(int argc, char *argv[])
{
	
   //get config
    if (configRead() == 2) configRead(); //condition 2 means a newfile was written
    //maybe if -1 then something unplanned happened and abort
    //or check that values exist in gconf
    
    
	int sockfd = socket(AF_INET, SOCK_STREAM, 0);
    nntpConnect(sockfd);
    nntpLogin(sockfd);
    nntpList(sockfd);
    nntpQuit(sockfd);    
}




int nntpConnect(fd)
{
    //constructs
    int n;
    struct sockaddr_in serv_addr;
    struct hostent *server;
    char buffer[256];


/* IPv4 AF_INET sockets:

struct sockaddr_in {
    short            sin_family;   // e.g. AF_INET, AF_INET6
    unsigned short   sin_port;     // e.g. htons(3490)
    struct in_addr   sin_addr;     // see struct in_addr, below
    char             sin_zero[8];  // zero this if you want to
}; */


    //config options
    //portno = gconf.port; //atoi(argv[2]);
    server = gethostbyname(gconf.server); //gethostbyname(argv[1]);

    //open socket or socket error handling
    if (fd < 0) 
        error("ERROR opening socket");
    if (server == NULL) {
        fprintf(stderr,"ERROR, no such host\n");
        exit(0);
    }

	memset((char *) &serv_addr, 0x00, sizeof(serv_addr));

    //populate sockaddr_in struct
    serv_addr.sin_family = AF_INET; 		//AF_INET or AF_INET6
    serv_addr.sin_port = htons(gconf.port);         //portno config
    //byte copy the server htonl to serv_addr.sin_addr to avoid errors
    bcopy((char *)server->h_addr, (char *)&serv_addr.sin_addr.s_addr, server->h_length);

    //try to connect or error out
    if (connect(fd,(struct sockaddr *) &serv_addr,sizeof(serv_addr)) < 0) 
        error("ERROR connecting");

    //connect message
    recv(fd, buffer, sizeof(buffer), 0);
    printf("%s", buffer);
	return 0;
}


//create login strings and pass them to nntpLine
int nntpLogin(fd)
{
	char usercmd[64];
	char passcmd[64];
	snprintf(usercmd, 64, "%s %s%s", "AUTHINFO USER", gconf.username, "\n");
	snprintf(passcmd, 64, "%s %s%s", "AUTHINFO PASS", gconf.password, "\n");
    nntpLine(fd, usercmd, 1);
    nntpLine(fd, passcmd, 0);
	return 0;
}

//runs a list command (stores to file) and quits
int nntpList(fd)
{
    nntpData(fd, "LIST\n");
    //store to file?
	return 0;
}

//quits NNTP
int nntpQuit(fd)
{
    nntpLine(fd, "QUIT\n", 1);
    close(fd);
}


//this function is used to get a single line of text
//a requirement is a \n at the end of the command!
//show is boolean for 1=show, 0=noshow (passwords)
int nntpLine(int fd, char *cmd, int show)
{

    //send a command
    if (show) 
		printf("%s", cmd);
    send(fd, cmd, strnlen(cmd, 32), 0); 

    //receive and output a response
    char buf[128];
    memset(buf, 0x00, sizeof(buf));
    
    recv(fd, buf, sizeof(buf), 0); 
    printf("%s", buf);
    
    return 0;
}

int nntpData (int fd, char *cmd)
{
    //this function fills the whole buffer with a packet
    //it uses poll for non-blocking operation and a 250ms timeout

    //poll setup
    struct pollfd pfds[0];
    pfds[0].fd = fd;
    pfds[0].events = POLLIN;
	
    //send a command
    printf ("%s", cmd);
    send (fd, cmd, strnlen(cmd, 32), 0); 

    //setup variables
    //int buf1 = malloc(sizeof(int) * 1500);
    unsigned int buffsize = 6000;
    unsigned int buf1[buffsize];	//stores characters
    unsigned int dataAvail = 1;	//0 when all data read
    signed int fdStatus = 0;    //holds fd status from poll

    while (dataAvail==1)
    {	
		//this is funny, the middle value is the number of pdfs objects, or pdfs[x+1]
		//if you have only pfds[0], indicating 1 pdfs, this is 1
		//if you have pfds[1] then that's 2 pdfs and 2 goes here
		fdStatus = poll(pfds, 1, 250); //0 = timeout expired, -1 is error, 1 is data ready
		switch (fdStatus)
		{
			case -1:
				//check if a period\n was received
				error ("while waiting for data, an error occured");
				dataAvail=0;
				break;
			case 0:
				printf ("timeout expired, if you see 1860573 below all is well\n");
				//this needs to check for 0x2E and 0x0A, if it's not true, start over
				dataAvail=0;
				break;
			default: //data is ready
			if (pfds[0].revents & POLLIN) 	//bitwise comparison to const 'POLLIN (see poll)'
				recv (fd, &buf1, buffsize, 0);	//get the data
				memset(buf1, 0x00, buffsize);   //clear the buffer, may be an inefficiency
			break;
			//if(buf1[0]==0x2E) //line starting with period (EOF)
		}
    }
    return 0;
}


//this function does the following in the following order:
//1) check if a config file exists, if it doesn't:
//	1a) gather the information needed
//	1b) write this to a file
//2) otherwise just read the config and poluate the global struct gconf
int configRead()
{

    //some byte work to conver this string to a char array
    char fileName[] = ".config";
    memcpy(gconf.fileConf, fileName, sizeof(fileName));

    //check for a config file, if there's not one, get the parameter
    //and then write them to a file
    //todo: error check the param?
    FILE *configFile;
	
    configFile = fopen(gconf.fileConf, "r");
    if (configFile==NULL) //if the config file doesn't exist, make one
    {	
        printf ("Could not open config file, making %s...\n", gconf.fileConf);
	printf ("You can press CTRL+C and create your own from %s.dist.\n", gconf.fileConf);
	printf ("Alternatively you can answer the questions below and populate the file:\n\n");

	printf ("username: ");
    	fgets (gconf.username, sizeof(gconf.username), stdin);

	printf ("password: ");
    	fgets (gconf.password, sizeof(gconf.password), stdin);
	
	printf ("server  : ");
    	fgets (gconf.server, sizeof(gconf.server), stdin);

	printf ("port    : ");
	scanf("%d", &gconf.port);

	printf ("maxconns: ");
	scanf("%d", &gconf.maxconns);

	printf ("username=%s\n password=%s\n server=%s\n maxconns=%d\n", 
	gconf.username,
	gconf.password,
	gconf.server,
	gconf.maxconns
	);	

	//if no file was found,
	//open the file for writing and place all content to it
    	configFile = fopen(gconf.fileConf, "w");
	if (configFile != NULL)
	{
	    int bytesWritten = 0;
	    fputs(gconf.username, configFile);   
	    fputs(gconf.password, configFile);   
	    fputs(gconf.server, configFile);   
	    fprintf(configFile, "%d\n", gconf.port);   
	    fprintf(configFile, "%d\n", gconf.maxconns);   
	    printf ("Wrote %d bytes to %s\n", bytesWritten, gconf.fileConf);
	    fclose (configFile);
	    return 2; //condition 2 indicates a new file was written.
	}
    } else {
        //read config

	fscanf(configFile, "%s", gconf.username);
	fscanf(configFile, "%s", gconf.password);   
	fscanf(configFile, "%s", gconf.server);   
	fscanf(configFile, "%d", &gconf.port);
	fscanf(configFile, "%d", &gconf.maxconns);   
        fclose (configFile);

	if(DEBUG)
		printf ("Using these settings:\n");
    if(DEBUG)
		printf (
	    "username=%s\npassword=%s\nserver=%s\nport=%d\nmaxconns=%d\n", 
	    gconf.username, gconf.password, gconf.server, gconf.port, gconf.maxconns
        );
	
    }
	return 0;
}
