#include "sorter_client.h"

#define BUFFER_SIZE 2048 // large enough buffer size

int switchValue;
char* columnType;
char *outputDirectory;
char *hostName;
char *portNumber;
pthread_t *tids;
int threadCount = 0;
int threadSize = 0;
int sockfd; // The global socket that will get used for the client
// int total = 0;
pthread_mutex_t socketLock = PTHREAD_MUTEX_INITIALIZER;

int main(int argc, char** argv) {
    if(argc != 7 && argc != 9 && argc != 11) { //Make sure input is valid.
    	outputErrorMessage("must enter 7, 9 or 11 arguments");
    }
    if((strcmp(argv[1], "-c") != 0) || (strcmp(argv[3], "-h") != 0) || (strcmp(argv[5], "-p") != 0)) {
    	outputErrorMessage("invalid argument input");
    }

    char* directoryName = (char*)malloc(sizeof(char)*BUFFER_SIZE);
    columnType = (char*)malloc(sizeof(char)*BUFFER_SIZE);
    outputDirectory = (char*)malloc(sizeof(char)*BUFFER_SIZE);
    hostName = (char*)malloc(sizeof(char)*BUFFER_SIZE);
    portNumber = (char*)malloc(sizeof(char)*BUFFER_SIZE);

    if (argc == 7) {
    	strcpy(columnType, argv[2]);
    	strcpy(hostName, argv[4]);
    	strcpy(portNumber, argv[6]);
    	strcpy(directoryName, ".");
    	strcpy(outputDirectory, ".");
    }
    else if (argc == 9) {
    	strcpy(columnType, argv[2]);
    	strcpy(hostName, argv[4]);
    	strcpy(portNumber, argv[6]);
    	strcpy(directoryName, argv[8]);
    	strcpy(outputDirectory, ".");
    }
    else if (argc == 11) {
    	strcpy(columnType, argv[2]);
    	strcpy(hostName, argv[4]);
    	strcpy(portNumber, argv[6]);
    	strcpy(directoryName, argv[8]);
    	strcpy(outputDirectory, argv[10]);
    }

    switchValue = switchVariable(columnType);

    fprintf(stderr, "Column Type  : [%s]\n", columnType);
    fprintf(stderr, "Column Number: [%d]\n", switchValue);
    fprintf(stderr, "Host Name    : [%s]\n", hostName);
    fprintf(stderr, "Port Number  : [%s]\n", portNumber);
    fprintf(stderr, "Directory    : [%s]\n", directoryName);
    fprintf(stderr, "Output       : [%s]\n\n", outputDirectory);

    /* Create a socket first */
    if((sockfd = socket(AF_INET, SOCK_STREAM, 0)) < 0) {
    	outputErrorMessage("could not create socket");
    }

    int s;
    struct addrinfo hints, *result;
    memset(&hints, 0, sizeof(struct addrinfo));
    hints.ai_family = AF_INET;
    hints.ai_socktype = SOCK_STREAM;

    if ((s = getaddrinfo(hostName, portNumber, &hints, &result)) != 0) {
    	outputErrorMessage("error with making sock");
    }

    /* Attempt a connection */
    if(connect(sockfd, result->ai_addr, result->ai_addrlen) == -1) {
    	outputErrorMessage("connect failed");
    }
    
    // Print copnnected socket
    struct sockaddr_in *result_addr = (struct sockaddr_in *)result->ai_addr;
    printf("Connected to IP %d on socket %d\n", ntohs(result_addr->sin_port), sockfd);

    // Create thread to search dir
    // threadSize = threadSize + 1;
    //tids = (pthread_t*)malloc(sizeof(pthread_t) * threadSize);
    pthread_t tid;
    pthread_create(&tid, NULL, traverseDirectory, directoryName);
    pthread_join(tid, NULL);
    threadCount = threadCount + 1;


    // TODO: Join the TIDs and make concurrent
    /*
    int x;
    for (x = 0; x < threadSize; x++) {
    	fprintf(stderr, "%lu, ", (unsigned long)tids[x]);
    	pthread_join(tids[x], NULL);
    }
    */

    printf("Number of Directories Searched: [%d]\n", threadCount);

    printf("DONE SENDING SORT REQUESTS\n");
 
    // Variables to send the request and recieve it
    char buffer[BUFSIZ];
    char colNum[3];
	ssize_t len;
    
    // Send the dump request
    strcat(buffer, "DUMP-");
    sprintf(colNum, "%d", switchValue);
    strcat(buffer, colNum);
	strcat(buffer, "-");
	strcat(buffer, columnType);
	len = send(sockfd, buffer, sizeof(buffer),0);
 
    // Get the sorted file from the server
    int file_size;
    recv(sockfd, buffer, BUFSIZ, 0);
    file_size = atoi(buffer);
    memset(buffer,0,sizeof(buffer));
    printf("File Size: %d\n", file_size);
	int remaining_data = 0;

	// Make CSV file to retrieve
	FILE *csv_file = fopen(attachSorted(), "ab+");
	if (csv_file == NULL) {
        printf("Failed to open sorted file.\n");
        exit(EXIT_FAILURE);
    }
    remaining_data = file_size;
	printf("GETTING FILE FROM SERVER\n");
    while ((remaining_data > 0) && ((len = recv(sockfd, buffer, BUFSIZ, 0)) > 0)){	
    	fwrite(buffer, sizeof(char), len, csv_file);
		memset(buffer,0,sizeof(buffer));
        remaining_data -= len;
    }

	fclose(csv_file);
 
	printf("DONE GETTING SORTED FILE... CLOSING SOCKET\n");

    close(sockfd);
    return 0;
}

void* traverseDirectory(void* arg) {
  // Get dir args
	char* directoryName = (char*)malloc(strlen((char*) arg) + 1);
	strcpy(directoryName, (char*) arg);
	printf("Directory Name: [%s]\n", directoryName);
	DIR *dirStream;
	struct dirent *dir;
	dirStream = opendir(directoryName);

	if(dirStream == NULL) {
		outputErrorMessage("incorrect path");
	}

	// Read the directory stream
	while ((dir = readdir(dirStream)) != NULL) {
		char* newPath = (char*)malloc(strlen(directoryName)+strlen(dir->d_name)+ 3);
		strcpy(newPath, attachName(directoryName, dir->d_name));
		if (((strcmp(".", dir->d_name)) == 0) || ((strcmp("..", dir->d_name)) == 0)) {
			continue;
		}
		if (dir->d_type == 4) {
			if (strstr(dir->d_name, ".") != NULL) {
				continue;
			}
			else {
			threadSize = threadSize + 1;
			// tids = (pthread_t*)realloc(tids, sizeof(pthread_t) * threadSize);
			pthread_t tid;
			pthread_create(&tid, NULL, traverseDirectory, newPath);
			pthread_join(tid, NULL);
			threadCount = threadCount + 1;
			}
		}
		else if (strstr(dir->d_name, ".csv") && dir->d_type == 8) {
			if (checkRepeat(dir->d_name) == 1) {
				if (checkCSV(newPath) == 1) {
					printf("CSV Found: [%s]\n", newPath);
					// Get a lock and call the sendRequest function
					pthread_mutex_lock(&socketLock);
					sendRequest(newPath);
					pthread_mutex_unlock(&socketLock);
				}
				else {
					outputErrorMessage("invalid csv file");
				}
			}
		}
	}

	closedir(dirStream);
	pthread_exit(0);
}

void outputErrorMessage(char *error) { // Standard output error function
	fprintf(stderr, "ERROR: %s\n", error);
	exit(0);
}

void sendRequest(char *fileName) {
    ssize_t len;
    int sent_bytes = 0;
    off_t offset;
    int remain_data;

    FILE *csv = fopen(fileName, "r");
    struct stat file_stat;
    fstat(fileno(csv), &file_stat);
    offset = 0;
    remain_data = file_stat.st_size;

	// Get file size
    fseek(csv, 0, SEEK_END);
    char file_size[256];
    sprintf(file_size, "%d", file_stat.st_size);

    // Sending file size
    len = send(sockfd, file_size, sizeof(file_size),0);
    printf("SENDING FILE: %s\n", fileName);
    while ((remain_data > 0) && ((sent_bytes = sendfile(sockfd, fileno(csv), &offset, BUFSIZ)) > 0))
    {
        remain_data -= sent_bytes;
    }

    printf("DONE WITH FILE: %s\n", fileName);

    fclose(csv);
}

char* attachName(const char* directoryName, const char* name) { 
	char* result;
	result = (char*)malloc(strlen(directoryName)+strlen(name)+ 3);
	if(!result) {
		outputErrorMessage("space not allocated");
	}

	strcpy(result, directoryName);
	strcat(result,"/");
	strcat(result, name);
	return result;
}

char* attachSorted() {
	char* name = (char*)malloc(9);
	strcpy(name, "AllFiles");

	char* output_filename = (char*) malloc(strlen(name) + strlen(columnType) + 13);
	char* s1 = "-sorted-";
	char* s2 = ".csv";
	strcpy(output_filename, name);
	strcat(output_filename, s1);
	strcat(output_filename, columnType);
	strcat(output_filename, s2);

	return output_filename;
}

int switchVariable(const char* columnType) {
	if(strcmp(columnType, "color") == 0) {
		return 0;
	}
	else if(strcmp(columnType, "director_name") == 0) {
		return 1;
	}
	else if(strcmp(columnType, "num_critic_for_reviews") == 0) {
		return 2;
	}
	else if(strcmp(columnType, "duration") == 0) {
		return 3;
	}
	else if(strcmp(columnType, "director_facebook_likes") == 0) {
		return 4;
	}
	else if(strcmp(columnType, "actor_3_facebook_likes") == 0) {
		return 5;
	}
	else if(strcmp(columnType, "actor_2_name") == 0) {
		return 6;
	}
	else if(strcmp(columnType, "actor_1_facebook_likes") == 0) {
		return 7;
	}
	else if(strcmp(columnType, "gross") == 0) {
		return 8;
	}
	else if(strcmp(columnType, "genres") == 0) {
		return 9;
	}
	else if(strcmp(columnType, "actor_1_name") == 0) {
		return 10;
	}
	else if(strcmp(columnType, "movie_title") == 0) {
		return 11;
	}
	else if(strcmp(columnType, "num_voted_users") == 0) {
		return 12;
	}
	else if(strcmp(columnType, "cast_total_facebook_likes") == 0) {
		return 13;
	}
	else if(strcmp(columnType, "actor_3_name") == 0) {
		return 14;
	}
	else if(strcmp(columnType, "facenumber_in_poster") == 0) {
		return 15;
	}
	else if(strcmp(columnType, "plot_keywords") == 0) {
		return 16;
	}
	else if(strcmp(columnType, "movie_imdb_link") == 0) {
		return 17;
	}
	else if(strcmp(columnType, "num_user_for_reviews") == 0) {
		return 18;
	}
	else if(strcmp(columnType, "language") == 0) {
		return 19;
	}
	else if(strcmp(columnType, "country") == 0) {
		return 20;
	}
	else if(strcmp(columnType, "content_rating") == 0) {
		return 21;
	}
	else if(strcmp(columnType, "budget") == 0) {
		return 22;
	}
	else if(strcmp(columnType, "title_year") == 0) {
		return 23;
	}
	else if(strcmp(columnType, "actor_2_facebook_likes") == 0) {
		return 24;
	}
	else if(strcmp(columnType, "imdb_score") == 0) {
		return 25;
	}
	else if(strcmp(columnType, "aspect_ratio") == 0) {
		return 26;
	}
	else if(strcmp(columnType, "movie_facebook_likes") == 0) {
		return 27;
	}
	else {
		outputErrorMessage("invalid column name");
		return 0;
	}
}

int checkCSV(char *filename) {
	FILE *fileCheck = fopen(filename, "r");
	char *firstLineCheck = (char*)malloc(sizeof(char)*BUFFER_SIZE);
	char *line = (char*)malloc(sizeof(char)*BUFFER_SIZE);
	if (fileCheck != NULL) {
		fgets(line, BUFFER_SIZE, fileCheck);
		strcpy(firstLineCheck, "color,director_name,num_critic_for_reviews,duration,director_facebook_likes,actor_3_facebook_likes,actor_2_name,actor_1_facebook_likes,gross,genres,actor_1_name,movie_title,num_voted_users,cast_total_facebook_likes,actor_3_name,facenumber_in_poster,plot_keywords,movie_imdb_link,num_user_for_reviews,language,country,content_rating,budget,title_year,actor_2_facebook_likes,imdb_score,aspect_ratio,movie_facebook_likes");
		strcat(firstLineCheck, "\r\n");
		if (strcmp(line, firstLineCheck) == 0) {
			fclose(fileCheck);
			return 1;
		}
		else {
			fclose(fileCheck);
			return 0;
		}
	}
	else {
		outputErrorMessage("file open error");
	}

	return 0;
}

int checkRepeat(char *name) {
	if (strstr(name, "-sorted") || strstr(name, "file_buffer.csv")) {
		return 0;
	}
	else {
		return 1;
	}
}

void gotoxy(int x, int y)
{
	printf("%c[%d;%df",0x1B,y,x);
}
