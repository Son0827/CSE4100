#include "csapp.h"

typedef struct{
    int maxfd;
    fd_set read_set;
    fd_set ready_set;
    int nready;
    int maxi;
    int clientfd[FD_SETSIZE];
    rio_t clientrio[FD_SETSIZE];
} pool;

typedef struct{
    int ID;
    int left;
    int price;
} item;

struct node{
    item data;
    struct node* left_child, *right_child;
};
typedef struct node* tree_pointer;

void init_pool(int listenfd, pool *p);
void add_client(int connfd, pool *p);
void check_clients(pool *p, tree_pointer root);
tree_pointer init_tree();
tree_pointer insert_node(tree_pointer root, tree_pointer node);
void free_node(tree_pointer root);
tree_pointer search(tree_pointer root, int ID);
void show(tree_pointer root, char* buf);
void buy(tree_pointer root, int ID, int num, char* buf);
void sell(tree_pointer root, int ID, int num, char* buf);
void sigint_handler(int sig);
void store_data(tree_pointer root, FILE* fp);

int byte_cnt = 0;
int listenfd;
tree_pointer root = NULL;

int main(int argc, char **argv)
{
    int connfd;
    socklen_t clientlen;
    struct sockaddr_storage clientaddr;
    static pool pool;

    Signal(SIGINT, sigint_handler);
    root = init_tree();

    if(argc != 2){
        fprintf(stderr, "usage: %s <port>\n", argv[0]);
        exit(0);
    }

    listenfd = Open_listenfd(argv[1]);
    init_pool(listenfd, &pool);

    while(1){
        pool.ready_set = pool.read_set;
        pool.nready = Select(pool.maxfd+1, &pool.ready_set, NULL, NULL, NULL);

        if(FD_ISSET(listenfd, &pool.ready_set)){
            clientlen = sizeof(struct sockaddr_storage);
            connfd = Accept(listenfd, (SA*)&clientaddr, &clientlen);
            add_client(connfd, &pool);
        }

        check_clients(&pool, root);
    }

}

void init_pool(int listenfd, pool *p)
{
    int i;
    p->maxi = -1;
    for (i=0; i<FD_SETSIZE; i++)
        p->clientfd[i] = -1;

    p->maxfd = listenfd;
    FD_ZERO(&p->read_set);
    FD_SET(listenfd, &p->read_set); 
}

void add_client(int connfd, pool *p)
{
    int i;
    p->nready--;
    for(i = 0; i < FD_SETSIZE; i++){
        if(p->clientfd[i] < 0){
            p->clientfd[i] = connfd;
            Rio_readinitb(&p->clientrio[i], connfd);

            FD_SET(connfd, &p->read_set);

            if(connfd > p->maxfd)
                p->maxfd = connfd;
            if(i > p->maxi)
                p->maxi = i;
            break;
        }
    }
    if(i == FD_SETSIZE)
        app_error("add_client error: Too many clients");
}

void check_clients(pool *p, tree_pointer root)
{
    int i, connfd, n;
    char buf[MAXLINE];
    char show_buf[MAXLINE] = {'\0'};
    char tmp_ptr[MAXLINE];
    rio_t rio;
    char* ptr = NULL;
    int ID, num;

    for(i = 0; (i <= p->maxi) && (p->nready > 0); i++){
        connfd = p->clientfd[i];
        rio = p->clientrio[i];
       
        if((connfd > 0) && (FD_ISSET(connfd, &p->ready_set))){
            p->nready--;
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
            Close(connfd);
            FD_CLR(connfd, &p->read_set);
            p->clientfd[i] = -1;
        }  
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
    if(!root) return NULL;
    if(root->data.ID == ID) return root;

    if(root->data.ID > ID) return search(root->left_child, ID);

    return search(root->right_child, ID);
}

void show(tree_pointer root, char* buf)
{
    if(!root) return;

    show(root->left_child, buf);
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
    show(root->right_child, buf);
}

void buy(tree_pointer root, int ID, int num, char* buf)
{
    tree_pointer tmp = search(root, ID);
    
    if(!tmp) return;

    if(tmp->data.left >= num){
        strcat(buf, "[buy] success\n");
        tmp->data.left -= num;
    }
    else{
        strcat(buf, "Not enough left stock\n");
    }
}

void sell(tree_pointer root, int ID, int num, char* buf)
{
    tree_pointer tmp = search(root, ID);

    if(!tmp) return;

    strcat(buf, "[sell] success\n");
    tmp->data.left += num;
}

void sigint_handler(int sig)
{
    Close(listenfd);
    FILE* fp = fopen("stock.txt", "w");
    store_data(root, fp);
    if(fp) fclose(fp);
    free_node(root);
    exit(0);
}

void store_data(tree_pointer root, FILE* fp){
    if(!root) return;

    fprintf(fp, "%d %d %d\n",root->data.ID, root->data.left, root->data.price);
    store_data(root->left_child, fp);
    store_data(root->right_child, fp);
}