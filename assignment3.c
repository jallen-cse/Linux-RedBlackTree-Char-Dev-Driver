#include <stdlib.h>
#include <stdio.h>
#include <unistd.h>
#include <fcntl.h>
#include <sys/stat.h>
#include <string.h>
#include <sys/ioctl.h>

#include <pthread.h>

#define CHECKOVERFLOW(timespec_obj) if(timespec_obj.tv_nsec >= 1000000000){timespec_obj.tv_sec=timespec_obj.tv_nsec/1000000000;timespec_obj.tv_nsec = timespec_obj.tv_nsec%1000000000;}

typedef struct rb_object {
    int key;
    char data[4];
} rb_object_t;

struct rb_packet {
    int key;
    int insert;
    char data[4];
};

struct file_tuple {
    FILE *infile;
    FILE *outfile;
};

//entry point of two parsing threads
void *parseScript(void *args)
{
    //open both character devices
    int rb_fds[2] = {open("/dev/rb438_dev1", O_RDWR), open("/dev/rb438_dev2", O_RDWR)};

    struct file_tuple *ft = (struct file_tuple*)args;

    FILE* script = ft->infile;
    FILE* outfile = ft->outfile;      //just printing for now
    //fputs("hello", outfile);

    //used to send to character device
    struct rb_packet packet, *recv_packet;
    char buf[13];   memset(buf, 0, 13);     //to ensure theres always a \n at end
    char obuf[16];  memset(obuf, 0, 16);     

    //parse script one line at time
    char *line, *token, *delim = " \n"; int rb, key;
    size_t line_len = 0;

    while(getline(&line, &line_len, script) != -1)
    {
        //command character of line        
        token = strtok(line, delim);

        //printf("%s\n", token);

        if (*token == 'w')        //write
        {
            token = strtok(NULL, delim);        
            rb = atoi(token);                   //which tree? (aka which device)

            key = atoi(strtok(NULL, delim));    //key of node
            token = strtok(NULL, delim);        //data string now
            
            packet.key = key;

            if (token == NULL)                          //in this case, we want to remove the key from the tree
            {
                packet.insert = 0;
                memset(&packet.data, 0, 4);
            }
            else                                        //in this case, we insert/update data into tree at key
            {
                packet.insert = 1;
                memcpy(&packet.data, token, 4);
            }

            //serialize into char array for write to character device
            memcpy(buf, &packet, 12);
            //printf("writing %d: %s\n", packet.key, packet.data);
            write(rb_fds[rb-1], buf, 12);

        }
        else if (*token == 'r')   //read
        {            
            if(read(rb_fds[atoi(strtok(NULL, delim))-1], buf, 12) == -1)    //if read return error
            {
                printf("No nodes in tree\n");
            }
            else
            {
                recv_packet = (struct rb_packet*)buf;                               //packet from char device

                sprintf(obuf, "%d: %s\n", recv_packet->key, recv_packet->data);     //output results to file

                //printf("read %d %s\n", recv_packet->key, recv_packet->data);//, recv_packet->data);
                fputs(obuf, outfile);
            }
        }
        else if (*token == 'd')   //sleep
        {
            struct timespec time = {0, 1000L*(long)atoi(strtok(NULL, delim))};
            CHECKOVERFLOW(time);

            //sleep thread
            nanosleep(&time, NULL);
        }
        else if (*token == 's')   //ioctl
        {
            rb = atoi(strtok(NULL, delim));

            //change head position
            int hp = atoi(strtok(NULL, delim));

            ioctl(rb_fds[rb-1], hp);//atoi(strtok(NULL, delim)));
            //printf("changed head pos to %d\n", hp);
        }
        else
        {
            //parsing failure
            close(rb_fds[0]);
            close(rb_fds[1]);

            exit(-1);
        }
    }

    close(rb_fds[0]);
    close(rb_fds[1]);

    return NULL;
}

int main(int argc, char** argv)
{
    if (argc < 3)
    {
        printf("Usage: ./assignment3 <script1> <script2>\n");
        exit(-1);
    }

    //files to read/write
    FILE *script1, *script2, *outfile1, *outfile2;

    //open files; these are passed as args to the threads
    script1 = fopen(argv[1], "r'");
    script2 = fopen(argv[2], "r'");
    outfile1 = fopen("output1", "w");
    outfile2 = fopen("output2", "w");

    //build arguments for pthread create 
    struct file_tuple* arg1 = (struct file_tuple*)malloc(sizeof(struct file_tuple));
    struct file_tuple* arg2 = (struct file_tuple*)malloc(sizeof(struct file_tuple));
    arg1->infile = script1;
    arg1->outfile = outfile1;
    arg2->infile = script2;
    arg2->outfile = outfile2;

    if (!script1 || !script2)
    {
        printf("At least one script could not be opened!\n");
        exit(-1);
    }

    //main thread attributes
    pthread_t main_id = pthread_self();                 
	struct sched_param main_sched_param = { 90 };       //main thread priority
    pthread_setschedparam(main_id, SCHED_FIFO, &main_sched_param);

    //script parsing thread attributes
    pthread_attr_t attr1, attr2;
    struct sched_param param1, param2;

    //initialize these attributes
    pthread_attr_init(&attr1);			
    pthread_attr_init(&attr2);		
    pthread_attr_setschedpolicy(&attr1, SCHED_FIFO);
    pthread_attr_setschedpolicy(&attr2, SCHED_FIFO);
    param1.sched_priority = 80;
    param1.sched_priority = 70;
    pthread_attr_setschedparam(&attr1, &param1);
    pthread_attr_setschedparam(&attr2, &param2);

    //create threads_ts
    pthread_t parse_thread1, parse_thread2;
    
    //start threads on parseScript function, pass FILE as argument
    pthread_create(&parse_thread1, &attr1, &parseScript, (void*)arg1);
    pthread_create(&parse_thread2, &attr2, &parseScript, (void*)arg2);

    //wait for parsing threads to exit
    pthread_join(parse_thread1, NULL);
    pthread_join(parse_thread2, NULL);

    //clear trees by reading all data
    int dev1_fd = open("/dev/rb438_dev1", O_RDWR);
    int dev2_fd = open("/dev/rb438_dev2", O_RDWR);

    char trash_buf[12];
    while(read(dev1_fd, trash_buf, 12) != -1) {};
    while(read(dev2_fd, trash_buf, 12) != -1) {};

    //restore head positions
    ioctl(dev1_fd, 0);
    ioctl(dev2_fd, 0);

    close(dev1_fd);
    close(dev2_fd);

    //deallocate arg structs
    free(arg1);
    free(arg2);

    return 0;
}