#include "csapp.h"
#include "sbuf.h"
#define NTHREADS 4
#define SBUFSIZE 16

sbuf_t sbuf;

typedef struct{
    int ID;
    int left;
    int price;
    int readcnt;
    sem_t w;
    sem_t r;
} item;

struct node{
    item data;
    struct node* left_child, *right_child;
};
typedef struct node* tree_pointer;

void echo_cnt(int connfd);
void *thread(void *vargp);
tree_pointer init_tree();
tree_pointer insert_node(tree_pointer root, tree_pointer node);
void free_node(tree_pointer root);
tree_pointer search(tree_pointer root, int ID);
void show(tree_pointer root, char* buf);
void buy(tree_pointer root, int ID, int num, char* buf);
void sell(tree_pointer root, int ID, int num, char* buf);
void sigint_handler(int sig);
void store_data(tree_pointer root, FILE* fp);

int listenfd;
tree_pointer root = NULL;
int readcnt = 0;

int main(int argc, char **argv)
{
    int i, connfd;
    socklen_t clientlen;
    struct sockaddr_storage clientaddr;
    pthread_t tid;

    Signal(SIGINT, sigint_handler);
    root = init_tree();

    if(argc != 2){
        fprintf(stderr, "usage: %s <port>\n", argv[0]);
        exit(0);
    }
    listenfd = Open_listenfd(argv[1]);

    sbuf_init(&sbuf, SBUFSIZE);
    for (i = 0; i < NTHREADS; i++)
        Pthread_create(&tid, NULL, thread, NULL);

    while(1){
        clientlen = sizeof(struct sockaddr_storage);
        connfd = Accept(listenfd, (SA*) &clientaddr, &clientlen);
        sbuf_insert(&sbuf, connfd);
    }
}

void *thread(void *vargp)
{
    Pthread_detach(pthread_self());
    while(1){
        int connfd = sbuf_remove(&sbuf);
        echo_cnt(connfd);
        Close(connfd);
    }
}


void echo_cnt(int connfd)
{
    int n;
    char buf[MAXLINE];
    char show_buf[MAXLINE] = {'\0'};
    char tmp_ptr[MAXLINE];
    rio_t rio;
    char* ptr = NULL;
    int ID, num;

    Rio_readinitb(&rio, connfd);
    while((n = Rio_readlineb(&rio, buf, MAXLINE)) != 0){
        printf("Server received %d bytes\n", (int)n);
        if(!strcmp(buf, "show\n")){
            show(root, show_buf);
            strcat(buf, show_buf);
            Rio_writen(connfd, buf, MAXLINE);             
        }
        else
        {
            strcpy(tmp_ptr, buf);

            ptr = strtok(buf, " ");
            if(!strcmp(ptr, "buy")){
                ptr = strtok(NULL, " ");
                ID = atoi(ptr);
                ptr = strtok(NULL, " \n");
                num = atoi(ptr);
                buy(root, ID, num, tmp_ptr);
                Rio_writen(connfd, tmp_ptr, MAXLINE);
            }
            else if(!strcmp(ptr, "sell")){
                ptr = strtok(NULL, " ");
                ID = atoi(ptr);
                ptr = strtok(NULL, " \n");
                num = atoi(ptr);
                sell(root, ID, num, tmp_ptr);
                Rio_writen(connfd, tmp_ptr, MAXLINE);
            }

        }
        memset(buf, 0 , sizeof(buf));
        memset(show_buf, 0, sizeof(show_buf));
        memset(tmp_ptr, 0, sizeof(tmp_ptr));
        ptr = NULL;
    }
}

tree_pointer init_tree(){
    FILE* fp = fopen("stock.txt", "r");
    int ID, left, price;
    tree_pointer root = NULL;

    while((fscanf(fp,"%d %d %d", &ID, &left, &price)) != EOF){
        tree_pointer newNode = (tree_pointer)malloc(sizeof(struct node));
        newNode->data.ID = ID;
        newNode->data.left = left;
        newNode->data.price = price;
        newNode->left_child = newNode->right_child = NULL;
        Sem_init(&newNode->data.r, 0, 1);
        Sem_init(&newNode->data.w, 0, 1); 
        newNode->data.readcnt = 0;
        root = insert_node(root, newNode);
    }

    fclose(fp);
    return root;
}

tree_pointer insert_node(tree_pointer root, tree_pointer node)
{
    if(!root){
        root = node;
        return root;
    } 

    if(node->data.ID > root->data.ID)
        root->right_child = insert_node(root->right_child, node);
    
    else if(node->data.ID < root->data.ID)
        root->left_child = insert_node(root->left_child, node);

    else{
        root->data.left = node->data.left;
        root->data.price = node->data.price;
    }

    return root;
}

void free_node(tree_pointer root)
{
    if(!root) return;

    free(root);

    free_node(root->left_child);
    free_node(root->right_child);
}

tree_pointer search(tree_pointer root, int ID)
{
    if(!root){
        return NULL;
    } 

    if(root->data.ID == ID){
        return root;
    }

    if(root->data.ID > ID){
        return search(root->left_child, ID);
    }

    return search(root->right_child, ID);
}

void show(tree_pointer root, char* buf)
{
    if(!root) return;

    show(root->left_child, buf);
    P(&root->data.r);
    root->data.readcnt++;
    if(readcnt == 1) P(&root->data.w);
    V(&root->data.r);
    char str[30];
    sprintf(str, "%d", root->data.ID);
	strcat(buf, str);
	strcat(buf, " ");
	sprintf(str, "%d", root->data.left);
	strcat(buf, str);
	strcat(buf, " ");
	sprintf(str, "%d", root->data.price);
	strcat(buf, str);
	strcat(buf, "\n");
    P(&root->data.r);
    root->data.readcnt--;
    if(root->data.readcnt == 0) V(&root->data.w);
    V(&root->data.r);
    show(root->right_child, buf);
}

void buy(tree_pointer root, int ID, int num, char* buf)
{   
    tree_pointer tmp = search(root, ID);
    
    if(!tmp){
        return;
    } 

    if(tmp->data.left >= num){
        P(&tmp->data.w);
        strcat(buf, "[buy] success\n");
        tmp->data.left -= num;
        V(&tmp->data.w);
    }
    else{
        strcat(buf, "Not enough left stock\n");
    }
}

void sell(tree_pointer root, int ID, int num, char* buf)
{
    tree_pointer tmp = search(root, ID);

    if(!tmp){
        return;
    }

    P(&tmp->data.w);
    strcat(buf, "[sell] success\n");
    tmp->data.left += num;
    V(&tmp->data.w);
}

void sigint_handler(int sig)
{
    Close(listenfd);
    FILE* fp = fopen("stock.txt", "w");
    store_data(root, fp);
    if(fp) fclose(fp);
    free_node(root);
    sbuf_deinit(&sbuf);
    exit(0);
}

void store_data(tree_pointer root, FILE* fp){
    if(!root) return;

    fprintf(fp, "%d %d %d\n",root->data.ID, root->data.left, root->data.price);
    store_data(root->left_child, fp);
    store_data(root->right_child, fp);
}