#include<iostream>
#include<fstream>
#include<vector>
#include<sys/time.h>
#include<sys/wait.h>
#include<unistd.h>
#include<signal.h>
#include<cstdio>
#include<cstdlib>
#include<cstring>
#include<cstdint>
#include<cmath>
#include<cctype>
#include<semaphore.h>
#include<queue>
#include<algorithm>
#include<pthread.h>
#include<iomanip>
using namespace std;
int g_thread_sum = 1;
struct job
{
    int l,r,m;
    string sort;
};

vector<int> A;
vector<pair<int,int> > range(8),range_level1(4),range_level2(2);
queue<job> job_queue,finish_job_queue;
sem_t sem_job;

pthread_mutex_t mutex_job_queue;
void add_merge_job_1();
void add_merge_job_2();
void add_merge_job_3();
void bubble_sort(int l, int r) { 
    for (int i = l; i < r - 1; ++i) 
    {
        for (int j = l; j < r - 1; ++j) 
        {
            if (A[j] > A[j + 1]) 
            swap(A[j], A[j + 1]);
        }
    }
}
void merge_sort(int l, int r, int m)
{
    vector<int> tmp;
    int i = l, j = m;
    while(i < m && j < r)
    {
        if(A[i] <= A[j])
        {
            tmp.push_back(A[i]);
            i++;
        }
        else
        {
            tmp.push_back(A[j]);
            j++;
        }
    }
    while(i < m)
    {
        tmp.push_back(A[i]);
        i++;
    }
    while(j < r)
    {
        tmp.push_back(A[j]);
        j++;
    }
    for(int k = 0; k < tmp.size(); k++)
    {
        A[l + k] = tmp[k];
    }
}
void *allwork(void* thread_id)
{
    while(1)
    {
    int id=*(int*)thread_id;
    sem_wait(&sem_job);
    pthread_mutex_lock(&mutex_job_queue);
    if(job_queue.empty())
    {
        pthread_mutex_unlock(&mutex_job_queue);
        sem_post(&sem_job); 
        continue;
    }
    job current_job = job_queue.front();
    job_queue.pop();
    pthread_mutex_unlock(&mutex_job_queue);
    if(current_job.l==-1 && current_job.r==-1)
    {
        break;
    }
    if(current_job.sort == "merge")
    {
        merge_sort(current_job.l, current_job.r, current_job.m);
    }
    else
    {
        bubble_sort(current_job.l, current_job.r);
    }
    
    pthread_mutex_lock(&mutex_job_queue);
    finish_job_queue.push(current_job);
    pthread_mutex_unlock(&mutex_job_queue);

    add_merge_job_1();
    add_merge_job_2();
    add_merge_job_3();
}
    return NULL;
}
void add_merge_job_1()
{
    pthread_mutex_lock(&mutex_job_queue);
    if(finish_job_queue.size()>=2 )
    {
        vector<job> temp_jobs;
        while(!finish_job_queue.empty())
        {
            temp_jobs.push_back(finish_job_queue.front());
            finish_job_queue.pop();
        }
        for(int i = 0; i < temp_jobs.size(); i++)
        {
            for(int j = i + 1; j < temp_jobs.size(); j++)
            {
                if(temp_jobs[i].l > temp_jobs[j].l)
                {
                    swap(temp_jobs[i], temp_jobs[j]);
                }
            }
        }
        int flag=0;
        for(int k = 0; k < 4; k++)
        {
            for(int i = 0; i < temp_jobs.size()-1; i++)
            {
                if(temp_jobs[i].l==range_level1[k].first && temp_jobs[i+1].r==range_level1[k].second && temp_jobs[i].r==temp_jobs[i+1].l)
                {
                    job merge_job;
                    merge_job.l = temp_jobs[i].l;
                    merge_job.m = temp_jobs[i].r;
                    merge_job.r = temp_jobs[i+1].r;
                    merge_job.sort = "merge";
                    job_queue.push(merge_job);
                    sem_post(&sem_job);
                    temp_jobs.erase(temp_jobs.begin() + i, temp_jobs.begin() + i + 2);
                    i--; 
                    flag=1;
                    
                    break;
                }
                
            }
            if(flag==1)
            {
                break;
            }
        }
        for(int k = 0; k < temp_jobs.size(); k++)
        {
            finish_job_queue.push(temp_jobs[k]);
        }
    }
    pthread_mutex_unlock(&mutex_job_queue);
}
void add_merge_job_2()
{
    pthread_mutex_lock(&mutex_job_queue);
    if(finish_job_queue.size()>=2 )
    {
        vector<job> temp_jobs;
        while(!finish_job_queue.empty())
        {
            temp_jobs.push_back(finish_job_queue.front());
            finish_job_queue.pop();
        }
        for(int i = 0; i < temp_jobs.size(); i++)
        {
            for(int j = i + 1; j < temp_jobs.size(); j++)
            {
                if(temp_jobs[i].l > temp_jobs[j].l)
                {
                    swap(temp_jobs[i], temp_jobs[j]);
                }
            }
        }
        int flag=0;
        for(int k = 0; k < 2; k++)
        {
            for(int i = 0; i < temp_jobs.size()-1; i++)
            {
                if(temp_jobs[i].l==range_level2[k].first && temp_jobs[i+1].r==range_level2[k].second && temp_jobs[i].r==temp_jobs[i+1].l)
                {
                    job merge_job;
                    merge_job.l = temp_jobs[i].l;
                    merge_job.m = temp_jobs[i].r;
                    merge_job.r = temp_jobs[i+1].r;
                    merge_job.sort = "merge";
                    job_queue.push(merge_job);
                    sem_post(&sem_job);
                    temp_jobs.erase(temp_jobs.begin() + i, temp_jobs.begin() + i + 2);
                    i--; 
                    flag=1;
                    break;
                }
                
            }
            if(flag==1)
            {
                break;
            }
    }
        for(int k = 0; k < temp_jobs.size(); k++)
        {
            finish_job_queue.push(temp_jobs[k]);
        }
        
    }
    pthread_mutex_unlock(&mutex_job_queue);
}
void add_merge_job_3()
{
    pthread_mutex_lock(&mutex_job_queue);
    if(finish_job_queue.size()==2)
    {
        vector<job> temp_jobs;
        while(!finish_job_queue.empty())
        {
            temp_jobs.push_back(finish_job_queue.front());
            finish_job_queue.pop();
        }
        for(int i = 0; i < temp_jobs.size(); i++)
        {
            for(int j = i + 1; j < temp_jobs.size(); j++)
            {
                if(temp_jobs[i].l > temp_jobs[j].l)
                {
                    swap(temp_jobs[i], temp_jobs[j]);
                }
            }
        }
        if (temp_jobs.size() == 2 &&temp_jobs[0].l == 0 &&temp_jobs[1].r == (int)A.size() &&temp_jobs[0].r == temp_jobs[1].l)     
        {
            
            job merge_job;
            merge_job.l = 0;
            merge_job.m = temp_jobs[0].r;
            merge_job.r = A.size();
            merge_job.sort = "merge";
            temp_jobs.pop_back();
            temp_jobs.pop_back();
            job_queue.push(merge_job);
            sem_post(&sem_job);
            for(int i = 0; i < g_thread_sum; i++)
            {
                job end_job;
                end_job.l = -1;
                end_job.r = -1;
                end_job.sort = "end";
                job_queue.push(end_job);
                sem_post(&sem_job);
            }
        }
       
        else 
        {
            for(int k = 0; k < temp_jobs.size(); k++)
            {
                finish_job_queue.push(temp_jobs[k]);
            }
        }
        
        }
    pthread_mutex_unlock(&mutex_job_queue);
}
void add_bubble_job()
{
    for(int i=0;i<8;i++)
    {
        job p_job;
        
        p_job.l=range[i].first;
        p_job.r=range[i].second;
        p_job.sort="bubble";
        pthread_mutex_lock(&mutex_job_queue);
        job_queue.push(p_job);
        pthread_mutex_unlock(&mutex_job_queue);
        sem_post(&sem_job);
    }
}
int main()
{
    sem_init(&sem_job,0,0);
    pthread_mutex_init(&mutex_job_queue, NULL);
    const int thread_sum=8;
    pthread_t threads[thread_sum];
    int thread_id[thread_sum];
    int rc;
    ifstream fin("input1.txt");
    int num; 
    string line;
    getline(fin, line); 
    while(fin >> num)
    {
        A.push_back(num);
    }
    fin.close();
    vector<int> cuts(9, 0);
    int n = (int)A.size();
    int base = n / 8, rem = n % 8;
    for (int i = 1; i <= 8; ++i) 
    {
    if (i <= rem)
        cuts[i] = cuts[i-1] + base + 1;
    else
        cuts[i] = cuts[i-1] + base;
    }

    for (int i = 0; i < 8; ++i) 
    {
        range[i].first  = cuts[i];
        range[i].second = cuts[i+1];
    }

    for (int i = 0; i < 4; ++i) 
    {
        range_level1[i].first  = range[2*i].first;
        range_level1[i].second = range[2*i+1].second;
    }
    range_level2[0].first  = range_level1[0].first;
    range_level2[0].second = range_level1[1].second;
    range_level2[1].first  = range_level1[2].first;
    range_level2[1].second = range_level1[3].second;
    vector<int> A0 = A;
    for (int thread_sum = 1; thread_sum <= 8; ++thread_sum) {
        A = A0;
        while (!job_queue.empty()) job_queue.pop();
        while (!finish_job_queue.empty()) finish_job_queue.pop();
        sem_destroy(&sem_job);
        sem_init(&sem_job, 0, 0);
        g_thread_sum = thread_sum;
        add_bubble_job();
        vector<pthread_t> threads(thread_sum);
        vector<int> thread_id(thread_sum);
        timeval start, t1;
        gettimeofday(&start, nullptr);

        for (int i = 0; i < thread_sum; i++) {
            thread_id[i] = i;
            pthread_create(&threads[i], NULL, allwork, (void*)&thread_id[i]);
        }
        for (int i = 0; i < thread_sum; i++) {
            pthread_join(threads[i], NULL);
        }

        gettimeofday(&t1, nullptr);
        double ms = (t1.tv_sec - start.tv_sec) * 1000.0 +
                    (t1.tv_usec - start.tv_usec) / 1000.0;

        cout << fixed << setprecision(6);
        cout << "worker thread #" << g_thread_sum << ", " << "elapsed= " << ms << " ms\n";
        string filename = "output_" + to_string(thread_sum) + ".txt";
        ofstream fout(filename);
        for (int i = 0; i < (int)A.size(); i++) {
            fout << A[i];
            if (i != (int)A.size() - 1) {
                fout << " ";
            }
        }
        fout.close();
    }    
}