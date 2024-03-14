#include "header.h"

movie_profile* movies[MAX_MOVIES];
unsigned int num_of_movies = 0;
unsigned int num_of_reqs = 0;

// Global request queue and pointer to front of queue
// TODO: critical section to protect the global resources
request* reqs[MAX_REQ];
int front = -1;

/* Note that the maximum number of processes per workstation user is 512. * 
 * We recommend that using about <256 threads is enough in this homework. */
pthread_t tid[MAX_CPU][MAX_THREAD]; //tids for multithread

#ifdef PROCESS
pid_t pid[MAX_CPU][MAX_THREAD]; //pids for multiprocess
#endif

//mutex
pthread_mutex_t lock; 

void initialize(FILE* fp);
request* read_request();
int pop();

int pop(){
	front+=1;
	return front;
}

//depth
int depth = 5;
int mergeThread = 32;

typedef struct input{
  char ** movies;
  double * pts;
  int len;
  int depth;
}input;

double Dot(double * a, double * b){
  double res = 0;
  for (int i = 0; i < NUM_OF_GENRE; i++)
    res += a[i] * b[i];
  return res;
}

void init_input(input * i, char ** m, double * p, int l, int d){
  i -> movies = m;
  i -> pts = p;
  i -> len = l;
  i -> depth = d;
}

#ifdef THREAD 
void * mergeSort(void * arg){
  input * Input = (input *)arg;
  input arg1, arg2;
  int len = Input -> len;
  int mid = len / 2;

  if (Input -> depth == depth - 1){
    sort(Input -> movies, Input -> pts, mid);
    sort(Input -> movies + mid, Input -> pts + mid, len - mid);
  }
  
  else{
    init_input(&arg1, Input -> movies, Input -> pts, mid, Input -> depth + 1);
    init_input(&arg2, Input -> movies + mid, Input -> pts + mid, len - mid, Input -> depth + 1);

    pthread_t tid1, tid2;
    if(pthread_create(&tid1, NULL, mergeSort, &arg1) != 0)
      ERR_EXIT("mergeSort");  
    if (pthread_create(&tid2, NULL, mergeSort, &arg2) != 0)
      ERR_EXIT("mergeSort");
    pthread_join(tid1, NULL);
    pthread_join(tid2, NULL);
  }

  int key1 = 0, key2 = mid, tmp_key = 0;
  char ** tmp_movies = (char **)malloc(sizeof(char *) * MAX_MOVIES);
  double * tmp_pts = (double *)malloc(sizeof(double) * MAX_MOVIES);

  while(key1 < mid && key2 < len){
    if (Input -> pts[key1] > Input -> pts[key2] || 
	(Input -> pts[key1] == Input -> pts[key2] && 
	 strcmp(Input -> movies[key1], Input -> movies[key2]) < 0)){

      tmp_movies[tmp_key] = Input -> movies[key1] ;
      tmp_pts[tmp_key] = Input -> pts[key1]; 
      key1 ++; tmp_key ++;
    }

    else{
      tmp_movies[tmp_key] = Input -> movies[key2] ;
      tmp_pts[tmp_key] = Input -> pts[key2]; 
      key2 ++; tmp_key ++;
    } 
  }

  while(key1 < mid){
      tmp_movies[tmp_key] = Input -> movies[key1] ;
      tmp_pts[tmp_key] = Input -> pts[key1]; 
      key1 ++; tmp_key ++;
  }

  while(key2 < len){
      tmp_movies[tmp_key] = Input -> movies[key2] ;
      tmp_pts[tmp_key] = Input -> pts[key2]; 
      key2 ++; tmp_key ++;
  }

  for (int i = 0; i < len; i++){
    Input -> movies[i] = tmp_movies[i];
    Input -> pts[i] = tmp_pts[i];
  }

  free(tmp_movies); free(tmp_pts);
  pthread_exit(NULL);
}

void Recommend(int cur){
	
	int len = 0;
	char ** filter = malloc(sizeof(char *) * MAX_MOVIES);
	double * pts = malloc(sizeof(double) * MAX_MOVIES);

        for (int i = 0; i < num_of_movies; i++){
          if (strstr(movies[i] -> title, reqs[cur] -> keywords) != NULL || 
	      strcmp(reqs[cur] -> keywords, "*") == 0){

	    filter[len] = movies[i] -> title;
	    pts[len] = Dot(reqs[cur] -> profile, movies[i] -> profile);
	    len ++;
	  }
	}	 
       	
	if (len < mergeThread)
	  sort(filter, pts, len);
	else{
          input Input;
	  pthread_t tid0;
	  init_input(&Input, filter, pts, len, 0); 
	  pthread_create(&tid0, NULL, mergeSort, &Input);
	  pthread_join(tid0, NULL);
	}

	char * buf = malloc(sizeof(char) * (MAX_LEN + 1));
	sprintf(buf, "%dt.out", reqs[cur] -> id); 
	FILE * fd = fopen(buf, "w");
	
	for (int i = 0; i < len; i++)
	  fprintf(fd, "%s\n", filter[i]);

	free(filter); free(pts);
}

void * Group(void * arg){

  int cur ;
  pthread_mutex_lock(&lock);
  pop();
  cur = front;
  pthread_mutex_unlock(&lock);

  while (cur < num_of_reqs){
      Recommend(cur);
      pthread_mutex_lock(&lock);
      pop();
      cur = front;
      pthread_mutex_unlock(&lock);
  }
  pthread_exit(NULL);
}

#elif defined PROCESS

void mergeSort(input * Input, int c){
  input arg1, arg2;
  int len = Input -> len;
  int mid = len / 2;
  
  //printf("Entering depth %d, len = %d\n", Input -> depth, len);

  char ** cpy_movies = (char **)malloc(sizeof(char *) * len);
  if (Input -> depth == depth - 1){
    memcpy(cpy_movies, Input -> movies, sizeof(char *) * len);
    sort(cpy_movies, Input -> pts, mid);
    sort(cpy_movies + mid, Input -> pts + mid, len - mid);
  }
  
  else{
    init_input(&arg1, Input -> movies, Input -> pts, mid, Input -> depth + 1);
    init_input(&arg2, Input -> movies + mid, Input -> pts + mid, len - mid, Input -> depth + 1);

    pid_t pid1, pid2;
    ((pid1 = fork()) && (pid2 = fork()));
    if (pid1 == 0){
      mergeSort(&arg1, 1);
      exit(0);
    }
    else if (pid2 == 0){
      mergeSort(&arg2, 2);
      exit(0);
    }
    else{
      wait(NULL); 
      wait(NULL);
      
      for (int i = 0; i < len; i++){
        cpy_movies[i] = (char *)malloc(sizeof(char) * MAX_LEN);
        strcpy(cpy_movies[i], Input -> movies[i]);	
      }
    }
  }

  int key1 = 0, key2 = mid, tmp_key = 0;
  char ** tmp_movies = (char **)malloc(sizeof(char *) * MAX_MOVIES);
  double * tmp_pts = (double *)malloc(sizeof(double) * MAX_MOVIES);

  while(key1 < mid && key2 < len){
    if (Input -> pts[key1] > Input -> pts[key2] || 
	(Input -> pts[key1] == Input -> pts[key2] && 
	 strcmp(cpy_movies[key1], cpy_movies[key2]) < 0)){

      tmp_movies[tmp_key] = cpy_movies[key1] ;
      tmp_pts[tmp_key] = Input -> pts[key1]; 
      key1 ++; tmp_key ++;
    }

    else{
      tmp_movies[tmp_key] = cpy_movies[key2] ;
      tmp_pts[tmp_key] = Input -> pts[key2]; 
      key2 ++; tmp_key ++;
    } 
  }

  while(key1 < mid){
      tmp_movies[tmp_key] = cpy_movies[key1] ;
      tmp_pts[tmp_key] = Input -> pts[key1]; 
      key1 ++; tmp_key ++;
  }

  while(key2 < len){
      tmp_movies[tmp_key] = cpy_movies[key2] ;
      tmp_pts[tmp_key] = Input -> pts[key2]; 
      key2 ++; tmp_key ++;
  }

  for(int i = 0; i < len; i++){
    strcpy(Input -> movies[i], tmp_movies[i]);
    Input -> pts[i] = tmp_pts[i];
  }

  //printf("Testing : movies[2] = %s, pts[2] = %lf\n", Input -> movies[2], Input -> pts[2]);
  free(tmp_movies); free(tmp_pts); free(cpy_movies);
/*
  char * buf = malloc(sizeof(char) * (MAX_LEN + 1));
  sprintf(buf, "%d.out", c); 
  int fd = open(buf, O_WRONLY | O_CREAT | O_TRUNC, 0644);
  for (int i = 0; i < len; i++){
    sprintf(buf, "%s %lf\n", Input -> movies[i], Input -> pts[i]);
    write(fd, buf, strlen(buf));
  }
*/
}

#endif

int main(int argc, char *argv[]){

	if(argc != 1){
#ifdef PROCESS
		fprintf(stderr,"usage: ./pserver\n");
#elif defined THREAD
		fprintf(stderr,"usage: ./tserver\n");
#endif
		exit(-1);
	}

	FILE *fp;

	if ((fp = fopen("./data/movies.txt","r")) == NULL){
		ERR_EXIT("fopen");
	}

	initialize(fp);
	assert(fp != NULL);
	fclose(fp);

#ifdef PROCESS
        pop();
	int len = 0;
	char ** filter = mmap(0, sizeof(char *) * MAX_MOVIES, PROT_READ | PROT_WRITE, MAP_ANONYMOUS | MAP_SHARED, -1, 0);
	double * pts = mmap(0, sizeof(double) * MAX_MOVIES, PROT_READ | PROT_WRITE, MAP_ANONYMOUS | MAP_SHARED, -1, 0);

        for (int i = 0; i < num_of_movies; i++){
          if (strstr(movies[i] -> title, reqs[front] -> keywords) != NULL || 
	      strcmp(reqs[front] -> keywords, "*") == 0){

	    filter[len] = mmap(0, sizeof(char) * MAX_LEN, PROT_READ | PROT_WRITE, MAP_ANONYMOUS | MAP_SHARED, -1, 0);
	    strcpy(filter[len], movies[i] -> title);
	    pts[len] = Dot(reqs[front] -> profile, movies[i] -> profile);
	    len ++;
	  }
	}

	if (len < mergeThread)
	  sort(filter, pts, len);
	else{
          input Input;
	  init_input(&Input, filter, pts, len, 0); 
	  mergeSort(&Input, 0);
	}

	char * buf = malloc(sizeof(char) * (MAX_LEN + 1));
	sprintf(buf, "%dp.out", reqs[front] -> id); 
	FILE * fd = fopen(buf,"w");
	
	for (int i = 0; i < len; i++){
	  fprintf(fd, "%s\n", filter[i]);
	}

#elif defined THREAD
       pthread_mutex_init(&lock, NULL);
       for (int i = 0 ; i < 256 / mergeThread; i++)
	 pthread_create(&tid[i][0], NULL, Group, NULL);
       for (int i = 0; i < 256 / mergeThread; i++)
         pthread_join(tid[i][0], NULL);

#endif 
	return 0;
}

/**=======================================
 * You don't need to modify following code *
 * But feel free if needed.                *
 =========================================**/

request* read_request(){
	int id;
	char buf1[MAX_LEN],buf2[MAX_LEN];
	char delim[2] = ",";

	char *keywords;
	char *token, *ref_pts;
	char *ptr;
	double ret,sum;

	scanf("%u %254s %254s",&id,buf1,buf2);
	keywords = malloc(sizeof(char)*strlen(buf1)+1);
	if(keywords == NULL){
		ERR_EXIT("malloc");
	}

	memcpy(keywords, buf1, strlen(buf1));
	keywords[strlen(buf1)] = '\0';

	double* profile = malloc(sizeof(double)*NUM_OF_GENRE);
	if(profile == NULL){
		ERR_EXIT("malloc");
	}
	sum = 0;
	ref_pts = strtok(buf2,delim);
	for(int i = 0;i < NUM_OF_GENRE;i++){
		ret = strtod(ref_pts, &ptr);
		profile[i] = ret;
		sum += ret*ret;
		ref_pts = strtok(NULL,delim);
	}

	// normalize
	sum = sqrt(sum);
	for(int i = 0;i < NUM_OF_GENRE; i++){
		if(sum == 0)
				profile[i] = 0;
		else
				profile[i] /= sum;
	}

	request* r = malloc(sizeof(request));
	if(r == NULL){
		ERR_EXIT("malloc");
	}

	r->id = id;
	r->keywords = keywords;
	r->profile = profile;

	return r;
}

/*=================initialize the dataset=================*/
void initialize(FILE* fp){

	char chunk[MAX_LEN] = {0};
	char *token,*ptr;
	double ret,sum;
	int cnt = 0;

	assert(fp != NULL);

	// first row
	if(fgets(chunk,sizeof(chunk),fp) == NULL){
		ERR_EXIT("fgets");
	}

	memset(movies,0,sizeof(movie_profile*)*MAX_MOVIES);	

	while(fgets(chunk,sizeof(chunk),fp) != NULL){
		
		assert(cnt < MAX_MOVIES);
		chunk[MAX_LEN-1] = '\0';

		const char delim1[2] = " "; 
		const char delim2[2] = "{";
		const char delim3[2] = ",";
		unsigned int movieId;
		movieId = atoi(strtok(chunk,delim1));

		// title
		token = strtok(NULL,delim2);
		char* title = malloc(sizeof(char)*strlen(token)+1);
		if(title == NULL){
			ERR_EXIT("malloc");
		}
		
		// title.strip()
		memcpy(title, token, strlen(token)-1);
	 	title[strlen(token)-1] = '\0';

		// genres
		double* profile = malloc(sizeof(double)*NUM_OF_GENRE);
		if(profile == NULL){
			ERR_EXIT("malloc");
		}

		sum = 0;
		token = strtok(NULL,delim3);
		for(int i = 0; i < NUM_OF_GENRE; i++){
			ret = strtod(token, &ptr);
			profile[i] = ret;
			sum += ret*ret;
			token = strtok(NULL,delim3);
		}

		// normalize
		sum = sqrt(sum);
		for(int i = 0; i < NUM_OF_GENRE; i++){
			if(sum == 0)
				profile[i] = 0;
			else
				profile[i] /= sum;
		}

		movie_profile* m = malloc(sizeof(movie_profile));
		if(m == NULL){
			ERR_EXIT("malloc");
		}

		m->movieId = movieId;
		m->title = title;
		m->profile = profile;

		movies[cnt++] = m;
	}
	num_of_movies = cnt;

	// request
	scanf("%d",&num_of_reqs);
	assert(num_of_reqs <= MAX_REQ);
	for(int i = 0; i < num_of_reqs; i++){
		reqs[i] = read_request();
	}
}
/*========================================================*/
