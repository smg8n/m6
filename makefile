

all: oss user

clean:
	-rm oss user logFile

dt:
	gcc -o user user.c -lpthread
	gcc -o oss oss.c -lpthread	
