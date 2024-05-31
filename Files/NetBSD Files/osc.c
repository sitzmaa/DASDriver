#include <unistd.h>
#include <fcntl.h>
#include <sys/ioctl.h>
#include "dasio.h"
#include <string.h>
#include <stdio.h>
#include <stdlib.h>

int
main()
{
  int data;
  int i;
  int option;
  int *buffer;
  int s;
  int length;
  int wait;
  
  int run =1;
  char file_name[50];
  FILE *outfd;

  int n, j;
  int dasfd;
  
  /*Initialize "Globals"*/
  
  dasfd = open("/dev/das0", O_RDWR, 0);
  if(dasfd <0) {
    fprintf(stderr, "osc: could not open /dev/das0\n");
    perror ("osc");
    return 1;
  }
  
  while(run)
    {
      
      printf("\n\n1.  Set Channel\n");
      printf("2.  Get Channel\n");
      printf("3.  Set Rate\n");
      printf("4.  Get Rate\n");
      printf("5.  Start Sampling\n");
      printf("6.  Stop Sampling\n");
      printf("7.  Read from Device\n");
      printf("8.  Osciliscope (DANGER)\n");
      printf("9.  Write Data to File\n");
      printf("10. Exit\n");
      printf("Select Option: ");
      s = scanf("%d", &option);

      if (s < 1) {
	option = 11;
	while (fgetc(stdin) != '\n' && !feof(stdin)) /* spin! */;
      }
      
      switch(option)
	{
	case 1:
	  printf("Enter a channel [0-7]: ");
	  scanf("%d", &data);
	  if( ioctl(dasfd, DAS_SET_CHANNEL, &data) != 0)
	    perror("Set Channel: ");
	  
	  break;

	case 2:
	  data = -1;
	  if( ioctl(dasfd, DAS_GET_CHANNEL, &data) == 0)
	    printf("\nChannel = 0x%x\n", data);
	  
	  else
	    perror("Get Channel: ");
	  
	  break;

	case 3:
	  printf("Select a rate: ");
	  scanf("%d", &data);
	  if( ioctl(dasfd, DAS_SET_RATE, &data) != 0)
	    perror("Set Rate: ");
	  
	  break;

	case 4:  
	  data = -1;
	  if( ioctl(dasfd, DAS_GET_RATE, &data) == 0)
	    printf("\nRATE = %d\n", data);
	  else
	    perror("Get Rate: ");
	  
	  break;

	case 5:  
	  if( ioctl(dasfd, DAS_START_SAMPLING, NULL) != 0)
	    perror("Start Sampling: ");
	  
	  break;

	case 6:
	  if( ioctl(dasfd, DAS_STOP_SAMPLING, NULL) != 0)
	    perror("Stop Sampling: ");
	  
	  break;

	case 7:
	  printf("How many data Points[0-1000]? ");
	  scanf("%d", &data);
	  buffer = (int *)malloc(sizeof(int) * data);
	  n = read(dasfd, buffer, (data * sizeof(int)));
	  n /= sizeof(int);
	  for(i = 0; i < n; i++)
	    printf("Time: %d  Value:0x%x\n", buffer[i]>>16, buffer[i]&0xffff);
	  
	  printf("Actually read %d\n", n);
	  free(buffer);
	  break;
	  
	case 10:
	  run = 0;
	  break;
	  
	case 8:
	  printf("You will HAVE to press CTRL-C to exit this mode!\n");
	  printf("How many data points should be retrieved at a time? ");
	  scanf("%d", &data);
	  s = data;
	  while(1)
	    {
	      buffer = (int *)malloc(sizeof(int) * s);
	      n = read(dasfd, buffer, (s * sizeof(int)));
	      n /= (sizeof(int));
	      for(data = 0; data<n; data++)
		{
		  length = ((buffer[data] & 0xfff)*80) / 0xfff;
		  for(j = 0; j < length; j++)
		    {
		      printf("-");
		    }
		  printf("*\n");
		  wait = 0;
		  while( wait < 1000)
		    wait++;
		  
		}
	      free(buffer);
	    }
	  break;
	  
	case 9:
	  printf("How many data points would you like? ");
	  scanf("%d", &data);
	  printf("What file would you like to write to? ");
	  scanf("%s", file_name);
	  outfd = NULL;
	  outfd = fopen(file_name, "w+");
	  if(NULL == outfd)
	    {
	      fprintf(stderr, "osc: could not open %s\n", file_name);
	      exit(1);
	    }
	  n = read(dasfd, buffer, data*(sizeof(int)));
	  for(i = 0; i< (n / sizeof(int)); i++)
	    {
	      fprintf(outfd, "[%d:0x%x]\n", buffer[i] >> 16, buffer[i] &0xffff);
	    }
	  fprintf(outfd, "Actually Read %d data Sets\n", n/sizeof(int));
	  fclose(outfd);
	  break;
	  
	default:
	  printf("Not an option\n");
	}
    }
  exit(0); 
  return 0;
}
