//#include <bits/stdc++.h> //msvc don't work with this.
#include <iostream>
#include <fstream>
#include <vector>
#include <list>
#include <iomanip>
#include <ctime>
#include <utility>
#include <tuple>
#include <string>
#include <sstream>
#include <math.h>
#define WIN32_LEAN_AND_MEAN
#include <windows.h>
#include <thread>
#include <future>
#include <atomic>
#include <chrono>
#include "imgui.h"
#include "imgui_stdlib.h"
#include "imgui_impl_glfw.h"
#include "imgui_impl_opengl3.h"
#include <glfw/glfw3.h> 
#include "llama.h" //llama model 
#ifndef LLM_MODEL_PATH
#define LLM_MODEL_PATH "models/Meta-Llama-3.1-8B-Instruct-Q8_0.gguf" //llama model Llama-3-13B-Instruct-Q4_0.gguf
#endif
using namespace std;

template<typename K,typename V> class Node{ //hand-write treap (A kind of BST) it's map version! 
	public:
		Node<K,V> *ch[2];
		int r=rand(),sz=1;
		K key; V val;
		int cmp(const K &x){
			if(x==key)
				return -1;
			return x<key?0:1;
		}
		void maintain(){
			sz=1;
			if(ch[0]!=nullptr) sz+=ch[0]->sz;
			if(ch[1]!=nullptr) sz+=ch[1]->sz;
		}
};

template<typename K,typename V> class treap{ //Besides, it supports find_by_order. All operations are avg. in logN !!
	private: 
		Node<K,V> *root=nullptr;
		void _rotate(Node<K,V>* &o,int d){
			Node<K,V> *k=o->ch[d^1]; o->ch[d^1]=k->ch[d]; k->ch[d]=o;
			o->maintain(); k->maintain(); o=k;
		}
		void _insert(Node<K,V>* &o,const K &key,const V &x){
			if(o==nullptr){
				o=new Node<K,V>();
				o->ch[0]=o->ch[1]=nullptr;
				o->key=key; o->val=x;
			}
			else if(key==o->key){
				o->key=key; o->val=x;
				return;
			}
			else{
				int d=o->cmp(key);
				_insert(o->ch[d],key,x);
				if(o->ch[d]->r > o->r)
					_rotate(o,d^1);
			}
			o->maintain();
		}
		void _remove(Node<K,V>* &o,const K &key){
			if(o==nullptr)
				return;
			int d=o->cmp(key);
			if(d==-1){
				if(o->ch[0]==nullptr){
					Node<K,V>* tmp=o;
					o=o->ch[1];
					delete tmp;
				}
				else if(o->ch[1]==nullptr){
					Node<K,V>* tmp=o;
					o=o->ch[0];
					delete tmp;
				}
				else{
					int d2=(o->ch[0]->r > o->ch[1]->r ?1:0);
					_rotate(o,d2); _remove(o->ch[d2],key);
				}
			}
			else
				_remove(o->ch[d],key);
			if(o!=nullptr)
				o->maintain();
		}
		V* _find(Node<K,V>* o,const K &key){
			while(o!=nullptr){
				int d=o->cmp(key);
				if(d==-1)
					return &o->val;
				else
					o=o->ch[d];
			}
			return nullptr;
		}
		pair<K*,V*> kth(Node<K,V>* o,int k){
			if(o==nullptr||k<=0||k > o->sz) return make_pair(nullptr,nullptr);
			int s=(o->ch[0]==nullptr?0:o->ch[0]->sz);
			if(k==s+1) return make_pair(&o->key,&o->val);
			else if(k<=s) return kth(o->ch[0],k);
			else return kth(o->ch[1],k-s-1);
		}
		void clear(Node<K,V>* o) {
			if(o==nullptr) return;
			clear(o->ch[0]);
			clear(o->ch[1]);
			delete o;
		}
	public:
		void insert(K key,V x){
			_insert(root,key,x);
		}
		void remove(K key){
			_remove(root,key);
		}
		V* find(K key){
			return _find(root,key);
		}
		pair<K*,V*> find_by_order(int k){ //1st min
			return kth(root,k);
		}
		int size(){
			if(root!=nullptr)
				root->maintain();
			return (root==nullptr?0:root->sz);
		}
		~treap(){
			clear(root);
		}
};


list<tuple<string,double,int> > toast_info;
void ShowToast(const string& msg, float duration = 2.0f, int type=0){
    toast_info.push_back(make_tuple(msg,ImGui::GetTime()+duration,type));
}

bool partial_match(string &a, string &b){ // a in b, KMP algorithm O(n+m) 
	int m=a.length(),n=b.length();
	if(m==0)
		return true;
	if(m>n)
		return false;
	vector<int> f(m+5); f[0]=-1;
	int j=-1;
	for(int i=0;i<m;){
		if(j==-1||a[i]==a[j]){
			i++; j++;
			f[i]=(i<m&&a[i]==a[j]?f[j]:j);
		}
		else
			j=f[j];
	}
	j=0;
	for(int i=0;i<n;i++){
		while(j>=0&&a[j]!=b[i])
			j=f[j];
		j++;
		if(j==m)
			return true; 
	}
	return false;
}

int today_date(int n=0){ //n days before
	auto now = chrono::system_clock::now() - chrono::days{n};
	time_t t = chrono::system_clock::to_time_t(now);
	tm local_tm;
	localtime_s(&local_tm, &t); //windows
	stringstream out_ss;
	out_ss << put_time(&local_tm, "%Y%m%d");
	return stoi(out_ss.str());
}

class Book{
	private:
		static int idseq;
	public:
		string title,genre,publisher,overview;
		string author[3];
		int year,copies,copies_left,id,author_num;
		int rating,borrow_time; //rating have to be divided by 10
		Book() : title(""), genre(""), publisher(""), overview(""), year(0), copies(0), copies_left(0), id(-1), author_num(0), rating(0) {
		}
		Book(const string &_title,const string &_genre,const string &_publisher,const string &_overview,const string (&_authors)[3],int _author_num,int _year,int _total_copies,int _rating): 
			title(_title), genre(_genre), publisher(_publisher), overview(_overview), author_num(_author_num), year(_year), copies(_total_copies), copies_left(_total_copies), rating(_rating){
				for (int i=0; i<3; i++){
					if(i<_author_num)
        				author[i]=_authors[i];
        			else
        				author[i]="";
				}
				id=idseq++;
				borrow_time=0;
			}
};
int Book::idseq=0;

class Library{
	private:
		treap<int,Book> order_data;
		treap<pair<string,int>,int> name_data;
		treap<string,treap<pair<string,int>,int> > title_to_order,genre_to_order,author_to_order,publisher_to_order;
		treap<int,treap<pair<string,int>,int> > year_to_order,rating_to_order,borrow_time_to_order;
		treap<int,treap<string,pair<int,int> > > book_id_to_user;
		treap<tuple<int,string,int>,int > borrow_date_to_user_and_book_id;
	public:
		int total_inventory=0,total_available_number=0,total_borrowing_times=0,rating_sum=0,total_data_length=0;
		Book& get_by_id(int id){
			return *(order_data.find(id));
		}
		Book& get_by_lex_order(int k){
			return *(order_data.find(*(name_data.find_by_order(k).second)));
		}
		treap<pair<string,int>,int>* get_treap_by_year(int yr){
			return year_to_order.find(yr);
		}
		treap<pair<string,int>,int>* get_treap_by_title(string tmp){
			return title_to_order.find(tmp);
		}
		treap<pair<string,int>,int>* get_treap_by_genre(string tmp){
			return genre_to_order.find(tmp);
		}
		treap<pair<string,int>,int>* get_treap_by_author(string tmp){
			return author_to_order.find(tmp);
		}
		treap<pair<string,int>,int>* get_treap_by_publisher(string tmp){
			return publisher_to_order.find(tmp);
		}
		treap<string,pair<int,int> >* get_treap_by_book_id(int id){
			return book_id_to_user.find(id);
		}
		tuple<Book&,int,int,string> get_book_number_date_username_by_date_order(int k){
			pair<tuple<int,string,int>*,int* > tmppair=borrow_date_to_user_and_book_id.find_by_order(k);
			return tuple<Book&,int,int,string>(get_by_id(get<2>(*tmppair.first)),*(tmppair.second),get<0>(*tmppair.first),get<1>(*tmppair.first));
		}
		vector<int> get_every_id_by_title_match(string name_buf){
			vector<int> return_ordered_id;
			for(int i=1;i<=title_to_order.size();i++){
				pair<string*,treap<pair<string,int>,int>*> tmp=title_to_order.find_by_order(i);
				if(partial_match(name_buf,*(tmp.first))){
					treap<pair<string,int>,int> *tmptrp=tmp.second;
					for(int j=1;j<=tmptrp->size();j++)
						return_ordered_id.push_back(*(tmptrp->find_by_order(j).second));
				}
			}
			return return_ordered_id;
		}
		vector<int> get_every_id_by_genre_match(string name_buf){
			vector<int> return_ordered_id;
			for(int i=1;i<=genre_to_order.size();i++){
				pair<string*,treap<pair<string,int>,int>*> tmp=genre_to_order.find_by_order(i);
				if(partial_match(name_buf,*(tmp.first))){
					treap<pair<string,int>,int> *tmptrp=tmp.second;
					for(int j=1;j<=tmptrp->size();j++)
						return_ordered_id.push_back(*(tmptrp->find_by_order(j).second));
				}
			}
			return return_ordered_id;
		}
		vector<int> get_every_id_by_author_match(string name_buf){
			vector<int> return_ordered_id;
			for(int i=1;i<=author_to_order.size();i++){
				pair<string*,treap<pair<string,int>,int>*> tmp=author_to_order.find_by_order(i);
				if(partial_match(name_buf,*(tmp.first))){
					treap<pair<string,int>,int> *tmptrp=tmp.second;
					for(int j=1;j<=tmptrp->size();j++)
						return_ordered_id.push_back(*(tmptrp->find_by_order(j).second));
				}
			}
			return return_ordered_id;
		}
		vector<int> get_every_id_by_publisher_match(string name_buf){
			vector<int> return_ordered_id;
			for(int i=1;i<=publisher_to_order.size();i++){
				pair<string*,treap<pair<string,int>,int>*> tmp=publisher_to_order.find_by_order(i);
				if(partial_match(name_buf,*(tmp.first))){
					treap<pair<string,int>,int> *tmptrp=tmp.second;
					for(int j=1;j<=tmptrp->size();j++)
						return_ordered_id.push_back(*(tmptrp->find_by_order(j).second));
				}
			}
			return return_ordered_id;
		}
		int size(){
			return order_data.size();
		}
		int borrower_size(){
			return borrow_date_to_user_and_book_id.size();
		}
		int add(const string &title,const string &genre,const string &publisher,const string &overview,const string (&authors)[3],int author_num,int year,int total_copies,int rating=50,int cp_left=-1,int brw_tm=0){
			Book tmp(title,genre,publisher,overview,authors,author_num,year,total_copies,rating);
			tmp.copies_left=(cp_left==-1?tmp.copies_left:cp_left);
			tmp.borrow_time=brw_tm;
			order_data.insert(tmp.id,tmp);
			name_data.insert(make_pair(tmp.title,tmp.id),tmp.id);
			if(title_to_order.find(tmp.title)==nullptr)
				title_to_order.insert(tmp.title,treap<pair<string,int>,int>());
			(title_to_order.find(tmp.title))->insert(make_pair(tmp.title,tmp.id),tmp.id);
			if(genre_to_order.find(tmp.genre)==nullptr)
				genre_to_order.insert(tmp.genre,treap<pair<string,int>,int>());
			(genre_to_order.find(tmp.genre))->insert(make_pair(tmp.title,tmp.id),tmp.id);
			for (int i=0; i<author_num; i++){
				if(author_to_order.find(authors[i])==nullptr)
					author_to_order.insert(authors[i],treap<pair<string,int>,int>());
				(author_to_order.find(authors[i]))->insert(make_pair(tmp.title,tmp.id),tmp.id);
			}
			if(publisher_to_order.find(tmp.publisher)==nullptr)
				publisher_to_order.insert(tmp.publisher,treap<pair<string,int>,int>());
			(publisher_to_order.find(tmp.publisher))->insert(make_pair(tmp.title,tmp.id),tmp.id);
			if(year_to_order.find(tmp.year)==nullptr)
				year_to_order.insert(tmp.year,treap<pair<string,int>,int>());
			(year_to_order.find(tmp.year))->insert(make_pair(tmp.title,tmp.id),tmp.id);
			if(rating_to_order.find(tmp.rating)==nullptr)
				rating_to_order.insert(tmp.rating,treap<pair<string,int>,int>());
			(rating_to_order.find(tmp.rating))->insert(make_pair(tmp.title,tmp.id),tmp.id);
			if(borrow_time_to_order.find(tmp.borrow_time)==nullptr)
				borrow_time_to_order.insert(tmp.borrow_time,treap<pair<string,int>,int>());
			(borrow_time_to_order.find(tmp.borrow_time))->insert(make_pair(tmp.title,tmp.id),tmp.id);
			total_inventory+=total_copies; total_available_number+=(cp_left==-1?total_copies:cp_left); rating_sum+=rating;
			total_borrowing_times+=brw_tm;
			total_data_length+=tmp.title.length()+tmp.genre.length()+tmp.publisher.length()+tmp.overview.length()+tmp.author[0].length()+tmp.author[1].length()+tmp.author[2].length()+to_string(total_copies).length()*2+36+tmp.author_num*3;
			return tmp.id;
		}
		bool borrow_book_id(int id,string username,int number,int brw_date=today_date(),bool needsign=true){
			Book& bk=get_by_id(id);
			if(username==""){
				ShowToast("lack borrower user name",3.0,0);
				return false;
			}
			if(number>bk.copies_left){
				ShowToast("Insufficient book stock",3.0,2);
				return false;
			}
			if(book_id_to_user.find(bk.id)==nullptr)
				book_id_to_user.insert(bk.id,treap<string,pair<int,int> >());
			else if(book_id_to_user.find(bk.id)->find(username)!=nullptr){
				ShowToast("User "+username+" haven\'t return this book.",3.0,2);
				return false;
			}
			book_id_to_user.find(bk.id)->insert(username,make_pair(number,brw_date));
			borrow_date_to_user_and_book_id.insert(make_tuple(brw_date,username,bk.id),number);
			(borrow_time_to_order.find(bk.borrow_time))->remove(make_pair(bk.title,bk.id));
			bk.copies_left-=number; bk.borrow_time+=number;
			if(borrow_time_to_order.find(bk.borrow_time)==nullptr)
				borrow_time_to_order.insert(bk.borrow_time,treap<pair<string,int>,int>());
			(borrow_time_to_order.find(bk.borrow_time))->insert(make_pair(bk.title,bk.id),bk.id);
			total_borrowing_times+=number; total_available_number-=number;
			if(needsign)
				ShowToast("Book borrowed by "+username+" successfully!",3,1);
			total_data_length+=username.length()+17;
			return true;
		}
		bool return_book_id(int id,string username){
			Book& bk=get_by_id(id);
			bk.copies_left++; total_available_number++;
			if(--(book_id_to_user.find(id)->find(username)->first)==0){
				borrow_date_to_user_and_book_id.remove(make_tuple(book_id_to_user.find(id)->find(username)->second,username,id));
				book_id_to_user.find(id)->remove(username);
			}
			else
				(*(borrow_date_to_user_and_book_id.find(make_tuple(book_id_to_user.find(id)->find(username)->second,username,id))))--;
			ShowToast("Book returned by "+username+" successfully!",3,1);
			total_data_length-=username.length()+17;
			return true;
		}
		void rating_book_id(int id,int new_rating){
			Book& bk=get_by_id(id);
			rating_sum-=bk.rating;
			(rating_to_order.find(bk.rating))->remove(make_pair(bk.title,bk.id));
			int tmp=(int)(((double)(bk.rating))*0.85+((double)(new_rating))*1.5);
			bk.rating=(tmp<100&&tmp>=94&&tmp==bk.rating?tmp+1:tmp);
			if(rating_to_order.find(bk.rating)==nullptr)
				rating_to_order.insert(bk.rating,treap<pair<string,int>,int>());
			(rating_to_order.find(bk.rating))->insert(make_pair(bk.title,bk.id),bk.id);
			rating_sum+=bk.rating;
		}
		vector<pair<string,int> > get_popular_book_data(){
			vector<pair<string,int> > tmp; int num=0;
			for(int i=0;num<5&&i<borrow_time_to_order.size();i++){
				pair<int*,treap<pair<string,int>,int>*> tmppair=borrow_time_to_order.find_by_order(borrow_time_to_order.size()-i);
				for(int j=0;num<5&&j<(tmppair.second->size());j++){
					tmp.push_back(make_pair((tmppair.second->find_by_order(tmppair.second->size()-j)).first->first,*(tmppair.first)));
					num++;
				}
			}
			return tmp;
		}
		vector<pair<string,int> > get_top_rated_book_data(){
			vector<pair<string,int> > tmp; int num=0;
			for(int i=0;num<5&&i<rating_to_order.size();i++){
				pair<int*,treap<pair<string,int>,int>*> tmppair=rating_to_order.find_by_order(rating_to_order.size()-i);
				for(int j=0;num<5&&j<(tmppair.second->size());j++){
					tmp.push_back(make_pair((tmppair.second->find_by_order(tmppair.second->size()-j)).first->first,*(tmppair.first)));
					num++;
				}
			}
			return tmp;
		}
		int get_total_book_number(){
			return order_data.size();
		}
		int get_total_genre_number(){
			return genre_to_order.size();
		}
		int get_total_publisher_number(){
			return publisher_to_order.size();
		}
		int get_total_author_number(){
			return author_to_order.size();
		}
		void Export_data_to_csv(){
			ofstream out("LRS_DB.csv");
			if(!out)
				throw runtime_error("fail");
			out<<borrower_size()<<'\n';
			for(int i=1;i<=borrower_size();i++){
				tuple<Book&,int,int,string> tmp_tuple=get_book_number_date_username_by_date_order(i);
				out<<(get<0>(tmp_tuple)).id<<' '<<(get<1>(tmp_tuple))<<' '<<(get<2>(tmp_tuple))<<' '<<quoted(get<3>(tmp_tuple))<<'\n';
			}
			int tmp_sz=order_data.size();
			for(int i=1;i<=tmp_sz;i++){
				Book &bk=*(order_data.find_by_order(i).second);
				string ov="";
				for(int i=0;i<bk.overview.length();i++){
					if(bk.overview[i]=='\n'){
						ov.push_back('\\');
						ov.push_back('n');
					}
					else
						ov.push_back(bk.overview[i]);
				}
				out<<quoted(bk.title)<<','<<quoted(bk.genre)<<','<<quoted(bk.publisher)<<','<<quoted(ov)<<','<<bk.author_num<<',';
				if(bk.author_num==1)
					out<<quoted(bk.author[0])<<',';
				else if(bk.author_num==2)
					out<<quoted(bk.author[0])<<','<<quoted(bk.author[1])<<',';
				else
					out<<quoted(bk.author[0])<<','<<quoted(bk.author[1])<<','<<quoted(bk.author[2])<<',';
				out<<bk.year<<','<<bk.copies<<','<<bk.copies_left<<','<<bk.rating<<','<<bk.borrow_time<<'\n';
			}
		}
		void Import_data_by_csv(){
			ifstream in("LRS_DB.csv");
			if(!in)
				throw runtime_error("fail");
			string tmp; string token[15];
			getline(in,tmp);
			int brwer_num=stoi(tmp);
			treap<int,vector<tuple<int,int,string> > > tmp_borrower;
			for(int i=0;i<brwer_num;i++){
				getline(in,tmp);
				stringstream ss(tmp); int tmp1,tmp2,tmp3; string tmpstr;
				ss>>tmp1>>tmp2>>tmp3>>quoted(tmpstr);
				if(tmp_borrower.find(tmp1)==nullptr)
					tmp_borrower.insert(tmp1,vector<tuple<int,int,string> >());
				tmp_borrower.find(tmp1)->push_back(make_tuple(tmp2,tmp3,tmpstr));
			}
			int cnt=0;
			while(getline(in,tmp)){
				stringstream ss(tmp); int ptr=0;
				while(ss){
					string field;
					if(ss.peek()=='\"')
						ss>>quoted(field);
					else
						getline(ss, field, ',');
					token[ptr++]=field;
					if(ss.peek()==',')
						ss.get();
				}
				string tmp_authors[3]={"","",""};
				for(int i=0;i<stoi(token[4]);i++)
					tmp_authors[i]=token[5+i];
				string ov="";
				for(int i=0;i<token[3].length();i++){
					if(i+1<token[3].length()&&token[3][i]=='\\'&&token[3][i+1]=='n'){
						ov.push_back('\n');
						i++;
					}
					else
						ov.push_back(token[3][i]);
				}
				int id=add(token[0],token[1],token[2],ov,tmp_authors,stoi(token[4]),stoi(token[5+stoi(token[4])]),stoi(token[6+stoi(token[4])]),stoi(token[8+stoi(token[4])]),-1,stoi(token[9+stoi(token[4])])); //last 2 stoi(token[7+stoi(token[4])])
				vector<tuple<int,int,string> > *tmpvec=tmp_borrower.find(cnt);
				if(tmpvec!=nullptr){
					for(int i=0;i<tmpvec->size();i++)
						borrow_book_id(id,get<2>((*tmpvec)[i]),get<0>((*tmpvec)[i]),get<1>((*tmpvec)[i]),false);
				}
				cnt++;
			}
		}
};

// Thread control for LLM and GUI
future<string> llama_future;
atomic<int> produced_tokens{0};

string call_llama(string prompt){
	static llama_model *model = nullptr;
	static llama_context *ctx = nullptr;
	static const llama_vocab *vocab = nullptr;
	static llama_sampler *sampler = nullptr;
	
	bool too_long = false;
	
    if(!ctx){
    	
    	// Get system thread number
		int n_logical=thread::hardware_concurrency();
		if(n_logical==0)
			n_logical=8;
		if(n_logical>18)
			n_logical=18;
		
		// load model and context
		llama_model_params mparams = llama_model_default_params();
		if(llama_supports_gpu_offload()){
			mparams.n_gpu_layers=32;
			mparams.main_gpu=0;
		}
		else
			mparams.n_gpu_layers=0;
		//Avoid huge amount of memory allocation
		mparams.use_mmap = false;
		mparams.use_mlock = false;
		model = llama_model_load_from_file(LLM_MODEL_PATH, mparams);
		if (!model) return "[load model failed]";
		
		llama_context_params cparams = llama_context_default_params();
		cparams.n_ctx=8192; //Context size. max. 8k for llama 3, 128k for llama 3.1 (tokens, 8k tokens allow around 10~40k of characters. This is enough for most cases)
		cparams.n_batch=8192; //Though this program use llama 3.1, for Vram consideration, I still use 8K only. (128k require more than 32GB of Vram)
		ctx = llama_init_from_model(model, cparams);
		if (!ctx) return "[create context failed]";
		
		llama_set_n_threads(ctx, n_logical-2, n_logical-4);
		
		// vocab and sampler chain setup
		vocab = llama_model_get_vocab(model);
		
		auto sparams = llama_sampler_chain_default_params();
		sampler = llama_sampler_chain_init(sparams);
        
        //Greedy sampling method
		/*llama_sampler_chain_reset(sampler);
		sampler = llama_sampler_init_dry();
		if (sampler) llama_sampler_free(sampler);
		llama_sampler_chain_add(sampler, llama_sampler_init_dry());*/
		// e.g. temperature -> top-k -> top-p -> deterministic
		
		//Standard probability sampling method
		llama_sampler_chain_add(sampler, llama_sampler_init_temp(0.5f));
		llama_sampler_chain_add(sampler, llama_sampler_init_top_k(60));
		llama_sampler_chain_add(sampler, llama_sampler_init_top_p(0.9f,1));
		llama_sampler_chain_add(sampler, llama_sampler_init_penalties(-1,1.2f,0.5f,0.3f));
		llama_sampler_chain_add(sampler, llama_sampler_init_dist(LLAMA_DEFAULT_SEED));
	}
	
	// clear previous result (memory)
	llama_kv_self_clear(ctx);
	llama_sampler_reset(sampler);
	
	// tokenize prompt
	vector<llama_token> tokens(8192); //8192 for llama 3, 128k for llama 3.1
	const bool is_first = llama_kv_self_used_cells(ctx) == 0;
	// get number of tokens
	int n_token=llama_tokenize(vocab, prompt.c_str(), (int)prompt.size(), tokens.data(), 7750, false, true);
	if(n_token<0){ // neg if tokenize fail (which means too long)
		too_long=true;
		n_token=7750;
	}
	vector<llama_token> tail(70);
	int tail_n = llama_tokenize(vocab,(too_long?"[too long, truncated...]<|eot_id|><|start_header_id|>assistant<|end_header_id|>":"<|eot_id|><|start_header_id|>assistant<|end_header_id|>"),(too_long?79:55),tail.data(),(int)tail.size(),false,true);
	memcpy(tokens.data() + (n_token>7750?7750:n_token), tail.data(), tail_n * sizeof(llama_token));
	n_token+=tail_n;
	
	// initial batch
	llama_batch batch = llama_batch_get_one(tokens.data(), n_token);
	
	// generation loop
	string output;
	const int max_predict = 300; //372 max (for llama3) 
	int decoded = 0;
	
	while (decoded < max_predict) {
		if (llama_decode(ctx, batch) != 0)
			return "[decode failed]";
		
		// sample next token
		llama_token id = llama_sampler_sample(sampler, ctx, -1);
		// if ppredicted token is end-of-generation token
		if (llama_vocab_is_eog(vocab, id))
			break;
		
		// detokenize
		char buf[256];
		int n_piece = llama_token_to_piece(vocab,id,buf,sizeof(buf),0,true);
		output.append(buf, n_piece);
		
		// next batch is just the new token
		batch = llama_batch_get_one(&id, 1);
		decoded++;
		produced_tokens++;
	}
	/*for(int i=0;i<output.length()-5;i++)
		if(output.substr(i,6)=="SYSTEM"||output.substr(i,3)=="END"){
			output=output.substr(0,i);
			break;
		}*/
	if(too_long)
		output="Too much data, I only received the truncated data.\n"+output;
	return output;
}


signed main(){
	//ios::sync_with_stdio(0); cin.tie(0); cout.tie(0);
	srand(time(nullptr)); //cout<<rand()<<' '<<rand();
	/*cout<<"OOPDS-HW2 Author:113550153\n"; //Console UI (Unused)
	cout<<"Library Record System here, welcome!\n";
	//string a; getline(cin,a); cout<<a.length()<<' '<<(a=="1111111\t");
	string input;
	cout<<"\nOperation list:\n1. Add a new book\n2. Search for a book by published year\n2. Search for a book by publisher\n2. Search for a book by author\n2. Search for a book by genre\n3. Check out a book\n4. Return a book\n5. List all books\n6. Exit\nEnter your choice: ";
	while(getline(cin,input)){
	}*/
	FreeConsole(); //Unbind console
	//llama test
	//cout<<"\n\n\nllama-3: "<<call_llama("This is test massage, respond to this massage if you received.")<<'\n'<<"End1\n\n"; 
	//cout<<call_llama("I have received your massage! Hi, I am Eason!")<<'\n'<<"End2\n";
	//::ShowWindow(::GetConsoleWindow(), SW_HIDE); //Conceal console (no use, already unbind console)
	
	//GUI shading, backend: OpenGL3, GLFW
	glfwInit(); //OpenGL3,GLFW initialization
	GLFWwindow* win = glfwCreateWindow(1440,720,"Library Record System by 113550153",0,0);
	glfwMakeContextCurrent(win);
	glfwSwapInterval(1); //v-sync on 
	IMGUI_CHECKVERSION();
	ImGui::CreateContext();
	ImGuiIO& io = ImGui::GetIO();
	io.ConfigFlags |= ImGuiConfigFlags_DockingEnable;
	ImGui_ImplGlfw_InitForOpenGL(win,true);
	ImGui_ImplOpenGL3_Init("#version 330"); //OpenGL version
	
	int init_animation=0;
	string init[3]={"Initialization......\nAuthentication: YES\nSystem loading.......\nHello, user.", "Welcome to the", "Library Record System"};
	//ImFont* ProggyClean12 = io.Fonts->AddFontFromFileTTF("imgui/misc/fonts/DroidSans.ttf", 12.0f);
	ImFont* SystemFont12 = io.Fonts->AddFontFromFileTTF("imgui/misc/fonts/Cousine-Regular.ttf", 13.0f);
	ImFont* Smorufont36 = io.Fonts->AddFontFromFileTTF("imgui/misc/fonts/Smoru.ttf", 36.0f);
	ImFont* ProggyClean24 = io.Fonts->AddFontFromFileTTF("imgui/misc/fonts/ProggyClean.ttf", 24.0f);
	
	Library MyLibrary;
	
	bool partial_match=false; int tmp_rate=5; int tmp_frame=0;
	string search_buf="",user_buf="",overview_buf="A fascinating book...",llm_output_buf=""; int show_book_num=10; int author_num=1;
	string author_buf[3]={"TA","Prof","Student"},title_buf="The Book",genre_buf="Good book",publisher_buf="NYCU",llm_input_buf="What might be the summary of the book with a title [Book Title]?";
	int year_buf=2025,stock_buf=3,rating_buf=5,current_page=1,popup_book=-1,chosen_user=0,user_current_page=1;
	const char* filter[] = {"Title", "Genre", "Author", "Publisher", "Year"}, *AI_filter[]={"Book Expert mode","Library Agent mode","General mode"}; int filter_choice=0,AI_filter_choice=0;
	const char* Advanced_filter[] = {"LLM Agent (Llama-3.1-8B-Instruct-Q8_0)", "The most popular books", "The top rated books", "Library book statistics", "Borrower record", "Database management"}; int Ad_filter_choice=0;
	const char* num1to5[] = {"1","2","3","4","5"}, *days_filter[]={"None","3 days ago","5 days ago","7 days ago","14 days ago","30 days ago"}; int borrow_number_buf=0,days_filter_choice=0;
	while(!glfwWindowShouldClose(win)){
		glfwPollEvents();
		ImGui_ImplGlfw_NewFrame();
		ImGui_ImplOpenGL3_NewFrame();
		ImGui::NewFrame();
		
		ImGuiViewport* viewport = ImGui::GetMainViewport();
		ImGuiID dockspace_id = ImGui::GetID("MyDockSpace"); //Set up dockspace
		ImGui::DockSpaceOverViewport(dockspace_id, viewport, ImGuiDockNodeFlags_None);
		
		if (glfwGetWindowAttrib(win, GLFW_ICONIFIED)){ //If GUI is minimized, skip render list and rendering 
			ImGui::Render();
			continue;
		}
		
		//Welcome Animation
		if(init_animation<=300){
			ImGui::SetNextWindowPos(ImVec2(700, 300),ImGuiCond_Always);
			ImGui::SetNextWindowSize(ImGui::GetIO().DisplaySize,ImGuiCond_Always);
			ImGui::Begin("##Fullscreen", nullptr,ImGuiWindowFlags_NoDecoration | ImGuiWindowFlags_NoBackground | ImGuiWindowFlags_NoInputs);
			ImGuiIO& io = ImGui::GetIO();
			if(init_animation<=150){
				ImGui::PushFont(ProggyClean24);
				int len=((init_animation*init[0].length()/140)+1<init[0].length()?(init_animation*init[0].length()/140)+1:init[0].length());
				ImGui::Text("%s",init[0].substr(0,len).c_str());
				ImGui::PopFont();
			}
			else{
				ImGui::PushFont(Smorufont36);
				ImGui::Text("%s",init[1].c_str());
				if(init_animation>=210)
					ImGui::Text("%s",init[2].c_str());
				ImGui::PopFont();
			}
			ImGui::End();
			init_animation++;
			ImGui::Render();
			signed w,h; glfwGetFramebufferSize(win,&w,&h);
			glViewport(0,0,w,h);
			glClearColor(0.1f,0.1f,0.1f,1);
			glClear(GL_COLOR_BUFFER_BIT);
			ImGui_ImplOpenGL3_RenderDrawData(ImGui::GetDrawData());
			glfwSwapBuffers(win);
			continue;
		}
		
		ImGui::SetNextWindowSizeConstraints(ImVec2(700, 300),ImVec2(1920, 1080));
		//ImGui::SetNextWindowSize(ImVec2(800, 600), ImGuiCond_FirstUseEver);
		ImGui::Begin("Book list"); bool search_change=false; int results_num; vector<Book> showlist;
		ImGui::AlignTextToFramePadding(); ImGui::Text("Search: "); ImGui::SameLine(); ImGui::SetNextItemWidth(ImGui::GetContentRegionAvail().x); search_change|=ImGui::InputText("##Title", &search_buf);
		ImGui::AlignTextToFramePadding(); ImGui::Text("Maximum book number to display: "); ImGui::SameLine(); ImGui::SetNextItemWidth(ImGui::GetContentRegionAvail().x); search_change|=ImGui::SliderInt("##Mbd", &show_book_num, 5, 50);
		ImGui::AlignTextToFramePadding(); ImGui::Text("Search type : "); ImGui::SameLine(); search_change|=ImGui::Combo("##filter", &filter_choice, filter, IM_ARRAYSIZE(filter));
		if(search_change)
			current_page=1;
		if(search_buf.length()>256)
			search_buf=search_buf.substr(0,256);
		if(filter_choice!=4){
			ImGui::SameLine(); ImGui::Checkbox("Partial match", &partial_match);
		}
		if(search_buf==""){
			results_num=MyLibrary.size();
			for(int i=(current_page-1)*show_book_num;i<current_page*show_book_num&&i<results_num;i++)
				showlist.push_back(MyLibrary.get_by_lex_order(i+1));
		}
		else{
			if(filter_choice==4){
				int tmp_year;
				try{
					tmp_year=stoi(search_buf);
				}catch(...){
					tmp_year=0;
				} 
				treap<pair<string,int>,int> *tmp_treap=MyLibrary.get_treap_by_year(tmp_year);
				if(tmp_treap!=nullptr){
					results_num=tmp_treap->size();
					for(int i=(current_page-1)*show_book_num;i<current_page*show_book_num&&i<results_num;i++)
						showlist.push_back(MyLibrary.get_by_id(*(tmp_treap->find_by_order(i+1).second)));
				}
				else
					results_num=0;
			}
			else{
				if(partial_match){
					vector<int> show_id;
					if(filter_choice==0)
						show_id=MyLibrary.get_every_id_by_title_match(search_buf);
					else if(filter_choice==1)
						show_id=MyLibrary.get_every_id_by_genre_match(search_buf);
					else if(filter_choice==2)
						show_id=MyLibrary.get_every_id_by_author_match(search_buf);
					else if(filter_choice==3)
						show_id=MyLibrary.get_every_id_by_publisher_match(search_buf);
					results_num=show_id.size();
					for(int i=(current_page-1)*show_book_num;i<current_page*show_book_num&&i<results_num;i++)
						showlist.push_back(MyLibrary.get_by_id(show_id[i]));
				}
				else{
					treap<pair<string,int>,int> *tmp_treap;
					if(filter_choice==0)
						tmp_treap=MyLibrary.get_treap_by_title(search_buf);
					else if(filter_choice==1)
						tmp_treap=MyLibrary.get_treap_by_genre(search_buf);
					else if(filter_choice==2)
						tmp_treap=MyLibrary.get_treap_by_author(search_buf);
					else if(filter_choice==3)
						tmp_treap=MyLibrary.get_treap_by_publisher(search_buf);
					if(tmp_treap!=nullptr){
						results_num=tmp_treap->size();
						for(int i=(current_page-1)*show_book_num;i<current_page*show_book_num&&i<results_num;i++)
							showlist.push_back(MyLibrary.get_by_id(*(tmp_treap->find_by_order(i+1).second)));
					}
					else
						results_num=0;
				}
			}
		}
		int total_page=((results_num/show_book_num)+(results_num%show_book_num>0?1:0)>0?(results_num/show_book_num)+(results_num%show_book_num>0?1:0):1);
		ImGui::AlignTextToFramePadding(); ImGui::Text("%d result(s) found, page %d/%d",results_num,current_page,total_page);
		ImGui::SameLine(); bool prevp = ImGui::Button("Previous page"); ImGui::SameLine(); bool nextp = ImGui::Button("Next page");
		ImGui::BeginTable("Books", 8, ImGuiTableFlags_Borders|ImGuiTableFlags_RowBg|ImGuiTableFlags_SizingFixedFit);
		ImGui::PushStyleColor(ImGuiCol_Text, IM_COL32(50, 255, 255, 255));
		ImGui::TableSetupColumn("      Title      ");
		ImGui::TableSetupColumn("    Genre    ");
		ImGui::TableSetupColumn("   Author(s)   ");
		ImGui::TableSetupColumn("   Publisher   ");
		ImGui::TableSetupColumn(" Year ");
		ImGui::TableSetupColumn(" Total stock ");
		ImGui::TableSetupColumn(" Available copies ");
		ImGui::TableSetupColumn("  Rating  ");
		ImGui::TableHeadersRow();
		ImGui::PopStyleColor();
		
		if(prevp)
			current_page=(current_page-1>=1?current_page-1:current_page);
		else if(nextp)
			current_page=(current_page+1<=total_page?current_page+1:current_page);
		
		for(int i=0;i<showlist.size();i++){
			ImGui::PushID(i);
			const Book &bk = showlist[i];
			ImGui::TableNextRow();
			ImGui::TableNextColumn(); ImGui::TextWrapped("%s", bk.title.c_str()); bool touch_book=ImGui::IsItemClicked();
			//ImGui::SameLine(); bool touch_book=ImGui::InvisibleButton("##click_title", ImGui::CalcTextSize(bk.title.c_str()));
			ImGui::TableNextColumn(); ImGui::TextWrapped("%s", bk.genre.c_str());
			string str_author=bk.author[0]+(bk.author_num>1?"\n"+bk.author[1]:"")+(bk.author_num>2?"\n"+bk.author[2]:"");
			ImGui::TableNextColumn(); ImGui::TextWrapped("%s", str_author.c_str());
			ImGui::TableNextColumn(); ImGui::TextWrapped("%s", bk.publisher.c_str());
			ImGui::TableNextColumn(); ImGui::Text("%d", bk.year);
			ImGui::PushStyleColor(ImGuiCol_Text,IM_COL32(255, 200, 10, 255));
			ImGui::TableNextColumn(); ImGui::Text("%d", bk.copies);
			ImGui::PopStyleColor();
			ImGui::PushStyleColor(ImGuiCol_Text, (bk.copies_left<=1?IM_COL32(255, 10, 10, 255):(bk.copies_left==2||bk.copies_left==3?IM_COL32(255, 255, 0, 255):IM_COL32(10, 255, 10, 255))));
			ImGui::TableNextColumn(); ImGui::Text("%d", bk.copies_left);
			ImGui::PopStyleColor();
			ImGui::PushStyleColor(ImGuiCol_Text, (bk.rating>=90?IM_COL32(255, 50, 10, 255):(bk.rating>=75?IM_COL32(255, 120, 20, 255):(bk.rating>=60?IM_COL32(120, 255, 0, 255):(bk.rating>=45?IM_COL32(100, 205, 100, 225):(bk.rating>=30?IM_COL32(255, 255, 120, 200):IM_COL32(120, 120, 255, 150)))))));
			ImGui::TableNextColumn(); ImGui::Text("%d.%d", bk.rating/10, bk.rating%10); //ImGui::Text("%d", (touch_book?1:0));
			ImGui::PopStyleColor();
			ImGui::TableNextColumn();
			ImGui::PopID();
			if(touch_book){
				popup_book=bk.id;
				chosen_user=0;
				borrow_number_buf=0;
				user_buf="";
				tmp_rate=5;
			}
	    }
		ImGui::EndTable();
		ImGui::End();
		
		ImGui::SetNextWindowSize(ImVec2(580, 360), ImGuiCond_Always);
		ImGui::Begin("New book register", nullptr, ImGuiWindowFlags_NoResize);
		ImGui::AlignTextToFramePadding(); ImGui::Text("Title: "); ImGui::SameLine(); ImGui::SetNextItemWidth(ImGui::GetContentRegionAvail().x); ImGui::InputText("##Book name", &title_buf);
		ImGui::AlignTextToFramePadding(); ImGui::Text("Genre: "); ImGui::SameLine(); ImGui::SetNextItemWidth(ImGui::GetContentRegionAvail().x); ImGui::InputText("##Genre name", &genre_buf);
		ImGui::AlignTextToFramePadding(); ImGui::Text("Number of Author(s): "); ImGui::SameLine(); ImGui::SetNextItemWidth(ImGui::GetContentRegionAvail().x); ImGui::SliderInt("##Author num", &author_num, 1, 3);
		ImGui::AlignTextToFramePadding(); ImGui::Text(" Author1: "); ImGui::SameLine(); ImGui::SetNextItemWidth(100.0f); ImGui::InputText("##author1", &(author_buf[0])); ImGui::SameLine(); ImGui::Text(" Author2: "); ImGui::SameLine(); ImGui::SetNextItemWidth(100.0f); ImGui::InputText("##author2", &(author_buf[1])); ImGui::SameLine(); ImGui::Text(" Author3: "); ImGui::SameLine(); ImGui::SetNextItemWidth(100.0f); ImGui::InputText("##author3", &(author_buf[2]));
		ImGui::AlignTextToFramePadding(); ImGui::Text(" Publisher:  "); ImGui::SameLine(); ImGui::SetNextItemWidth(ImGui::GetContentRegionAvail().x); ImGui::InputText("##Publisher name", &publisher_buf);
		ImGui::BeginGroup();
		ImGui::AlignTextToFramePadding(); ImGui::Text("    Year:    "); ImGui::SameLine(); ImGui::SliderInt("##Year num", &year_buf, 1800, 2100);
		ImGui::AlignTextToFramePadding(); ImGui::Text("Total stock: "); ImGui::SameLine(); ImGui::SliderInt("##stock num", &stock_buf, 0, 100);
		ImGui::AlignTextToFramePadding(); ImGui::Text("   Rating:   "); ImGui::SameLine(); ImGui::SliderInt("##rate num", &rating_buf, 1, 10);
		ImGui::EndGroup();
		ImGui::SameLine();
		bool add_book=ImGui::Button("Add", ImVec2(80, 65));
		ImGui::Text("Overview:");
		ImGui::SetNextItemWidth(-1);
		ImGui::InputTextMultiline("##overview text", &overview_buf, ImVec2(0, 0), ImGuiInputTextFlags_AllowTabInput);
		if(add_book){
			if(title_buf==""||genre_buf==""||publisher_buf==""||author_buf[0]==""||(author_num>1&&author_buf[1]=="")||(author_num>2&&author_buf[2]==""))
				ShowToast("lack info!",3,0);
			else if(title_buf.length()>256||genre_buf.length()>256||publisher_buf.length()>256||author_buf[0].length()>256||(author_num>1&&author_buf[1].length()>256)||(author_num>2&&author_buf[2].length()>256))
				ShowToast("The main attribute string can't exceed 256 characters!",3,2);
			else{
				MyLibrary.add(title_buf,genre_buf,publisher_buf,overview_buf,author_buf,author_num,year_buf,stock_buf,rating_buf*10);
				title_buf=""; genre_buf=""; publisher_buf=""; overview_buf="A fascinating book..."; author_buf[0]=""; author_buf[1]=""; author_buf[2]="";
				author_num=1; year_buf=2000; stock_buf=3; rating_buf=5;
				ShowToast("Book added",3,1);
			}
		} //ImGui::Text("%d",(int)add_book); ImGui::Text("%d", toast_info.size());
		ImGui::End();
		
		ImVec2 center = ImGui::GetMainViewport()->GetCenter();
		string target_user="";
		
		ImGui::SetNextWindowSize(ImVec2(580, 360), ImGuiCond_Always);
		ImGui::Begin("Advanced operation", nullptr, ImGuiWindowFlags_NoResize);
		ImGui::SetNextItemWidth(300); ImGui::Combo("##Adfilter", &Ad_filter_choice, Advanced_filter, IM_ARRAYSIZE(Advanced_filter));
		if(Ad_filter_choice==0){
			ImGui::SameLine(); ImGui::Text("    "); ImGui::SameLine(); ImGui::SetNextItemWidth(210.0f); ImGui::Combo("##AIfilter", &AI_filter_choice, AI_filter, IM_ARRAYSIZE(AI_filter));
			ImGui::Text("\n");
			ImGui::InputTextMultiline("##llm input text", &llm_input_buf, ImVec2(500, 60), ImGuiInputTextFlags_AllowTabInput);
			ImGui::SameLine();
			if(ImGui::Button("Send",ImVec2(60, 60))){
				ImGui::OpenPopup("AI Agent");
				tmp_frame=0;
			}
			//if(llm_input_buf.length()>400)
			//	llm_input_buf=llm_input_buf.substr(0,400);
			ImGui::SetNextWindowPos(center, ImGuiCond_Appearing, ImVec2(0.5f, 0.5f));
			ImGui::SetNextWindowSize(ImVec2(600, 400), ImGuiCond_Appearing);
			if (ImGui::BeginPopupModal("AI Agent",nullptr,ImGuiWindowFlags_NoMove | ImGuiWindowFlags_NoResize | ImGuiWindowFlags_NoCollapse)){
				ImGui::Text("\n\n\n\n\n\n\n\n\n                         Generating... Please be patient.");
				if(tmp_frame==0){
					string prompt="<|begin_of_text|>"; //Meta llama ChatML standard format.
					if(AI_filter_choice==0)
						prompt+="<|start_header_id|>system<|end_header_id|> You are an expert on various books, but you know nothing about library system. YOU CAN\'T ANSWER ANY QUESTION RELATED TO LIBRARY RECORD SYSTEM. ANSWER THE USER's QUESTION in 4-7 sentences, using only plain ASCII.\n<|eot_id|>";
					else if(AI_filter_choice==2){
						prompt+="<|start_header_id|>system<|end_header_id|> You are the System Assistant for a C++/OpenGL Library Record System (LRS) with built-in AI. If the user asks how to use the interface, explain the exact steps and warnings based on the instructions below. DO NOT INVENT ANY OPERATION OR METHOD, YOU CAN SAY NO SUCH OPERATION. (If question is not relate to this system, just answer it(even question not related to system). If question is related, MUST follow the instruction) Reply in 4-7 sentences of ASCII only.\n<|eot_id|>";
						prompt+="<|start_header_id|>instruction<|end_header_id|>\n";
						prompt+="Overview:\n-Register books in New book register block(upper left panel) (filling title, genre, author(s) up to 3, publisher, year, stock, overview;).\n-View and search books in Book list block(right panel), user can search by title/genre/author/publisher/year (partial match can be enabled except year; blank searchbar shows all books).\n";
						prompt+="-In Book list block, display up to 50 results per page; click a book title to view details (such as views, overview, etc.).\n-Borrow/return: In the book detail window, enter borrower username, select the number of books to borrow, click Borrow/Return (buttons replaced by notice if no copies).\n-Rate books in book detail window (rating slider + \"Rate\" button; new rating = 15% weight).\n-view the top 5 popular books and the top 5 rated books by switch the combo box to \"The most popular books\" or \"The top rated books\" in advanced operation block(lower left panel).\n";
						prompt+="-View overall statistics in Advanced operation block(switch the combo box to \"Library book statistics\"): counts of books/genres/authors/publishers, total/available stock, borrow ratio, total borrows, avg rating.\nView Borrower Record in advanced operation block(switch the combo box to \"Borrower record\"): records sorted by date and than by username, click prev. page button or next page button to view other record.";
						prompt+="-Import/Export CSV in Advanced operation block(switch the combo box to \"Database management\"): file \"LRS_DB.csv\" in working folder; import may corrupt if file does not follow LRS format; export may overwrites existing file and results in file broken.\n";
						prompt+="-Exit system in Advanced operation block(switch the combo box to \"Database management\"): click \"Exit System\" to exit, which will clear all data - remind user to export library data first.\n";
						prompt+="AI Agent: local Llama-3.1-8B-Instruct-Q8_0, offline.  Modes (must choose correctly):\n-Book Expert: ask about any book, summaries, personalized recommendations.\n-Library Agent: analyze current database/booklist, recommendations, summaries.\n-General: system usage help or freeform Q and A. \n";
						prompt+="Important UI notes:\n-Subwindow layout is adjustable, but **do not combine or minimize** any subwindow (e.g. Booklist window); doing so will terminate the program.\n-Use the correct AI mode for the user\'s goal.\n";
						prompt+="<|eot_id|>";
					}
					else if(AI_filter_choice==1){
						prompt+="<|start_header_id|>system<|end_header_id|> You are a concise Library Information Agent. You MUST NOT repeat the question or ask new questions. ONLY ANSWER THE QUESTION BASED ON THE LIBRARY STYSTEM INFORMATION AND BOOKLIST DATA (which is a part of the library data) WITH A LITTLE INFERENCE OR SUGGESTION (BUT MUST BASED ON DATA, such as publisher...), in plain ASCII, 4-7 sentences. DO NOT INVENT ANY BOOK DATA. We don\'t use ISBN. Please think step by step. Verify your is answer consistent with provided data before output.\n<|eot_id|>";
						//for(int i=1;i<=MyLibrary.size()&&i<=1000;i++)
						//	Book &bk=MyLibrary.get_by_lex_order(i);
						prompt+="<|start_header_id|>library system information<|end_header_id|>\n";
						prompt+="- Total books number: "+to_string(MyLibrary.get_total_book_number())+"\n";
						prompt+="- Genres number: "+to_string(MyLibrary.get_total_genre_number())+"\n";
						prompt+="- Authors number: "+to_string(MyLibrary.get_total_author_number())+"\n";
						prompt+="- Publishers number: "+to_string(MyLibrary.get_total_publisher_number())+"\n";
						prompt+="- Total inventory: "+to_string(MyLibrary.total_inventory)+"\n";
						prompt+="- Total available book number: "+to_string(MyLibrary.total_available_number)+"\n";
						prompt+="- Borrow ratio: "+to_string((MyLibrary.total_inventory==0?0:(MyLibrary.total_inventory-MyLibrary.total_available_number)*100/MyLibrary.total_inventory))+"."+to_string((MyLibrary.total_inventory==0?0:(MyLibrary.total_inventory-MyLibrary.total_available_number)*10000/MyLibrary.total_inventory%100))+"\n";
						prompt+="- Borrowing times: "+to_string(MyLibrary.total_borrowing_times)+"\n";
						prompt+="- Average book rating(From 1 to 10): "+to_string((MyLibrary.get_total_book_number()==0?0:MyLibrary.rating_sum/MyLibrary.get_total_book_number()/10))+"."+to_string((MyLibrary.get_total_book_number()==0?0:MyLibrary.rating_sum/MyLibrary.get_total_book_number()%10))+"\n";
						prompt+="- Library database size(around): "+to_string(MyLibrary.total_data_length)+" bytes \n";
						prompt+="<|eot_id|>";
						prompt+="<|start_header_id|>booklist data<|end_header_id|>\n";
						for(int i=0;i<showlist.size()&&i<50;i++){
							Book &tmp_bk=showlist[i];
							prompt+=to_string(i+1)+". \""+tmp_bk.title+"\" ("+tmp_bk.genre+"), by "+(tmp_bk.author[0]==""?"None":tmp_bk.author[0])+", "+(tmp_bk.author[1]==""?"None":tmp_bk.author[1])+", "+(tmp_bk.author[2]==""?"None":tmp_bk.author[2])+", publisher: "+tmp_bk.publisher+", public. year: "+to_string(tmp_bk.year)+", total stock: "+to_string(tmp_bk.copies)+", Avail. stock: "+to_string(tmp_bk.copies_left)+", views: "+to_string(tmp_bk.borrow_time)+", rating: "+to_string(tmp_bk.rating/10)+"."+to_string(tmp_bk.rating%10)+"\n";
							prompt+="Overview:\n"+tmp_bk.overview+"\n\n";
						}
						if(showlist.size()==0)
							prompt+="No Data\n<|eot_id|>";
						else
							prompt+="<|eot_id|>";
					}
					prompt+="<|start_header_id|>user<|end_header_id|>";
					produced_tokens=0;
					llama_future=async(launch::async, call_llama, prompt+llm_input_buf);
					tmp_frame=1;
				}
				else if(tmp_frame==1){
					if(llama_future.valid()){
						if(llama_future.wait_for(chrono::milliseconds(0))==future_status::ready){
							llm_output_buf=(AI_filter_choice==1?"Because of LLM model's limited data accessibility, this answer might not be based on the entire book list information:\n":"")+llama_future.get()+"\n\n---------\n\n"+llm_output_buf; //"\n\nRESPONSE FORMAT: 3-4 brief sentences replies based on the above book data, plain ASCII, no extra commentary.\n"
							tmp_frame=2;
						}
						else{
							float frag=1.0f-1.0f/pow(1.022f,float(produced_tokens.load())); //Nonlinear approximation
							ImGui::Text("        "); ImGui::SameLine();
							ImGui::ProgressBar((frag>0.9999f?0.9999f:(frag<0.00001f?0.00001f:frag)), ImVec2(440,0), "##AIProgress");
						}
					}
				}
				else if(tmp_frame==2)
					ImGui::CloseCurrentPopup();
				ImGui::EndPopup();
			}
			ImGui::Text("\nLLM response:"); //Harry Potter
			int ptr=0,lastnl=0;
			while(ptr<llm_output_buf.length()){
				if(llm_output_buf[ptr]=='\n')
					lastnl=ptr;
				if(ptr-lastnl>76){ //Auto new line
					llm_output_buf=llm_output_buf.substr(0,ptr)+"\n"+llm_output_buf.substr(ptr);
					lastnl=ptr;
				}
				ptr++;
			}
			ImGui::InputTextMultiline("##llm output text", &llm_output_buf, ImVec2(570, 180), ImGuiInputTextFlags_AllowTabInput);
		}
		if(Ad_filter_choice==1||Ad_filter_choice==2){
			vector<float> bars;
			vector<pair<string,int> > tmp;
			if(Ad_filter_choice==1)
				tmp=MyLibrary.get_popular_book_data();
			else
				tmp=MyLibrary.get_top_rated_book_data();
			float mxv=0.0;
			for(int i=0;i<5;i++){
				if(i<tmp.size()){ 
					mxv=max(mxv,(((float)tmp[i].second)/(Ad_filter_choice==1?10.0f:1.0f)));
					bars.push_back((((float)tmp[i].second)/(Ad_filter_choice==1?10.0f:1.0f)));
				}
				else
					bars.push_back(0.0f);
			}
			ImGui::Text(""); ImGui::Text("");
			for (int i=0; i<tmp.size();i++){
				if(Ad_filter_choice==2){
					ImGui::SameLine(85 + i*95);
					ImGui::Text("%d.%d",tmp[i].second/10,tmp[i].second%10);
				}
				else{
					ImGui::SameLine(75 + i*95);
					ImGui::Text("%d",tmp[i].second);
				}
			}
			ImGui::Text("    "); ImGui::SameLine(); ImGui::PlotHistogram("##bars",bars.data(),(int)bars.size(),0,nullptr,0.0f,mxv+1.0f,ImVec2(480, 240));
			ImGui::Text("");
			for (int i=0; i<tmp.size();i++){
				ImGui::SameLine(60 + i*95);
				string book_title=tmp[i].first;
				if(book_title.length()>10)
					book_title=book_title.substr(0,9)+"~";
				ImGui::Text("%s", book_title.c_str());
			}
		}
		else if(Ad_filter_choice==3){
			ImGui::Text(""); ImGui::Text(""); 
			ImGui::Text("    Number of various books: %d", MyLibrary.get_total_book_number()); ImGui::SameLine(290); ImGui::Text("    Number of various genres: %d", MyLibrary.get_total_genre_number());
			ImGui::Text("");
			ImGui::Text("    Number of various authors: %d", MyLibrary.get_total_author_number()); ImGui::SameLine(290); ImGui::Text("    Number of various publishers: %d", MyLibrary.get_total_publisher_number());
			ImGui::Text("");
			ImGui::Text("    Total book inventory: %d", MyLibrary.total_inventory); ImGui::SameLine(290); ImGui::Text("    Number of available books: %d", MyLibrary.total_available_number);
			ImGui::Text("");
			ImGui::Text("    Borrowing ratio: %d.%d%%", (MyLibrary.total_inventory==0?0:(MyLibrary.total_inventory-MyLibrary.total_available_number)*100/MyLibrary.total_inventory), (MyLibrary.total_inventory==0?0:(MyLibrary.total_inventory-MyLibrary.total_available_number)*10000/MyLibrary.total_inventory%100));
			ImGui::Text("");
			ImGui::Text("    Total borrowing times: %d", MyLibrary.total_borrowing_times);
			ImGui::Text("");
			ImGui::Text("    The average book rating: %d.%d", (MyLibrary.get_total_book_number()==0?0:MyLibrary.rating_sum/MyLibrary.get_total_book_number()/10), (MyLibrary.get_total_book_number()==0?0:MyLibrary.rating_sum/MyLibrary.get_total_book_number()%10));
		}
		else if(Ad_filter_choice==5){
			ImGui::SameLine(); ImGui::Text("                  "); ImGui::SameLine();
			if(ImGui::Button("Exit System")){
				ImGui::End();
				break;
			}
			ImGui::Text("");
			ImGui::Text("  The .csv file should be named as LRS_DB.csv.");
			ImGui::Text("  Please ensure the imported data meet the LRS format."); ImGui::Text("");
			ImGui::Text("    "); ImGui::SameLine();
			if(ImGui::Button("Import from current folder",ImVec2(240, 60))){
				try{
					MyLibrary.Import_data_by_csv();
					ShowToast("Data has been imported!",3,1);
				}catch(...){
					ShowToast("Fail to import database file!",3,2);
				}
			}
			ImGui::Text("");
			ImGui::Text("  The exported .csv file will be named as LRS_DB.csv in LRS format.");
			ImGui::Text("  Note: This operation might overwrite the existed file!"); ImGui::Text("");
			ImGui::Text("    "); ImGui::SameLine();
			if(ImGui::Button("Export to current folder",ImVec2(240, 60))){
				try{
					MyLibrary.Export_data_to_csv();
					ShowToast("Data has been exported!",3,1);
				}catch(...){
					ShowToast("Fail to export database file!",3,2); 
				}
			}
			ImGui::Text("");
			ImGui::Text("  The estimated export data size: %d bytes",MyLibrary.total_data_length);
		}
		else if(Ad_filter_choice==4){
			ImGui::SameLine(); ImGui::Text(" Filter: "); ImGui::SameLine(); ImGui::SetNextItemWidth(180.0f); ImGui::Combo("##daysfilter", &days_filter_choice, days_filter, IM_ARRAYSIZE(days_filter));
			int day_num=(days_filter_choice==0?0:today_date(days_filter_choice==1?3:(days_filter_choice==2?5:(days_filter_choice==3?7:(days_filter_choice==4?14:(days_filter_choice==5?30:30))))));
			int each_user_page_num=10;
			int user_total_page=((MyLibrary.borrower_size()/each_user_page_num)+(MyLibrary.borrower_size()%each_user_page_num>0?1:0)>0?(MyLibrary.borrower_size()/each_user_page_num)+(MyLibrary.borrower_size()%each_user_page_num>0?1:0):1);
			//ImGui::Text("");
			ImGui::AlignTextToFramePadding(); ImGui::Text("%d record(s), page %d/%d",MyLibrary.borrower_size(),user_current_page,user_total_page); ImGui::SameLine(); ImGui::Text("  "); ImGui::SameLine();
			if(ImGui::Button("Previous page##2"))
				user_current_page=(user_current_page-1>=1?user_current_page-1:user_current_page);
			ImGui::SameLine(); ImGui::Text(" "); ImGui::SameLine();
			if(ImGui::Button("Next page##2"))
				user_current_page=(user_current_page+1<=user_total_page?user_current_page+1:user_current_page);
			ImGui::BeginTable("Borrower Record", 4, ImGuiTableFlags_Borders|ImGuiTableFlags_RowBg|ImGuiTableFlags_SizingFixedFit);
			ImGui::PushStyleColor(ImGuiCol_Text, IM_COL32(235, 255, 55, 255));
			ImGui::TableSetupColumn("          Book          ");
			ImGui::TableSetupColumn(" Number ");
			ImGui::TableSetupColumn("     Borrower(username)     ");
			ImGui::TableSetupColumn("  Borrow Date  ");
			ImGui::TableHeadersRow();
			ImGui::PopStyleColor();
			
			for(int i=(user_current_page-1)*each_user_page_num;i<user_current_page*each_user_page_num&&i<MyLibrary.borrower_size();i++){
				ImGui::PushID(i);
				tuple<Book&,int,int,string> tmptuple = MyLibrary.get_book_number_date_username_by_date_order(i+1);
				Book& bk=get<0>(tmptuple);
				ImGui::TableNextRow();
				ImGui::TableNextColumn(); ImGui::TextWrapped("%s", (bk.title).c_str()); bool touch_book=ImGui::IsItemClicked();
				ImGui::TableNextColumn(); ImGui::Text("%d", (get<1>(tmptuple)));
				ImGui::TableNextColumn(); ImGui::TextWrapped("%s", (get<3>(tmptuple)).c_str());
				ImGui::PushStyleColor(ImGuiCol_Text,((get<2>(tmptuple))<day_num?IM_COL32(255, 25, 25, 255):IM_COL32(205, 255, 205, 255)));
				ImGui::TableNextColumn(); ImGui::Text("  %d/%d/%d", (get<2>(tmptuple))/10000,((get<2>(tmptuple))/100)%100,(get<2>(tmptuple))%100);
				ImGui::PopStyleColor();
				ImGui::TableNextColumn();
				ImGui::PopID();
				if(touch_book){
					popup_book=bk.id;
					chosen_user=0;
					borrow_number_buf=0;
					user_buf=""; target_user=get<3>(tmptuple);
					tmp_rate=5;
				}
		    }
			ImGui::EndTable();
		}
		ImGui::End();
		
		ImGui::SetNextWindowPos(center, ImGuiCond_Appearing, ImVec2(0.5f, 0.5f));
		ImGui::SetNextWindowSize(ImVec2(600, 400), ImGuiCond_Appearing);
		if(popup_book!=-1)
	    	ImGui::OpenPopup("Book detail");
		if(ImGui::BeginPopupModal("Book detail",nullptr,ImGuiWindowFlags_NoMove | ImGuiWindowFlags_NoResize | ImGuiWindowFlags_NoCollapse)){
			Book &bk=MyLibrary.get_by_id(popup_book);
			ImGui::Text("Title: %s", bk.title.c_str());
			ImGui::Text("Genre: %s", bk.genre.c_str());
			string str_author=bk.author[0]+(bk.author_num>1?"\n           "+bk.author[1]:"")+(bk.author_num>2?"\n           "+bk.author[2]:"");
			ImGui::Text("Author(s): %s", str_author.c_str());
			ImGui::Text("Publisher: %s", bk.publisher.c_str());
			ImGui::Text("Year: %d", bk.year);
			ImGui::PushStyleColor(ImGuiCol_Text,IM_COL32(255, 200, 10, 255));
			ImGui::Text("Total stock: %d", bk.copies);
			ImGui::PopStyleColor();
			ImGui::PushStyleColor(ImGuiCol_Text, (bk.copies_left<=1?IM_COL32(255, 10, 10, 255):(bk.copies_left==2||bk.copies_left==3?IM_COL32(255, 255, 0, 255):IM_COL32(10, 255, 10, 255))));
			ImGui::Text("Availible number: %d", bk.copies_left);
			ImGui::PopStyleColor();
			ImGui::PushStyleColor(ImGuiCol_Text, (bk.rating>=90?IM_COL32(255, 50, 10, 255):(bk.rating>=75?IM_COL32(255, 120, 20, 255):(bk.rating>=60?IM_COL32(120, 255, 0, 255):(bk.rating>=45?IM_COL32(100, 205, 100, 225):(bk.rating>=30?IM_COL32(255, 255, 120, 200):IM_COL32(120, 120, 255, 150)))))));
			ImGui::Text("Rating: %d.%d", bk.rating/10, bk.rating%10);
			ImGui::PopStyleColor();
			ImGui::Text("Number of views: %d", bk.borrow_time);
			ImGui::TextWrapped("Overview:\n%s\n\n", bk.overview.c_str());
			
			ImGui::AlignTextToFramePadding(); ImGui::Text(" User(Borrower): "); ImGui::SameLine(); ImGui::SetNextItemWidth(240); ImGui::InputText("##user borrower", &user_buf); ImGui::SameLine(); ImGui::Text("    Number: "); ImGui::SameLine(); ImGui::SetNextItemWidth(60); ImGui::Combo("##borrow number filter", &borrow_number_buf, num1to5, IM_ARRAYSIZE(num1to5));
			
			treap<string,pair<int,int> >* user_treap=MyLibrary.get_treap_by_book_id(bk.id);
			vector<const char*> bookuser;
			vector<string> bookusername,bookuserdisplay; //bookuserdisplay is a tmp memory for bookuser to access, because combo box only allow const str* array.
			if(user_treap!=nullptr)
				for(int i=1;i<=user_treap->size();i++){
					string str_tmp=((*(user_treap->find_by_order(i).first))+" ("+to_string(user_treap->find_by_order(i).second->second)+")  -  "+to_string(user_treap->find_by_order(i).second->first));
					bookuserdisplay.push_back(str_tmp);
					bookuser.push_back(bookuserdisplay[i-1].c_str());
					bookusername.push_back(*(user_treap->find_by_order(i).first));
					if(target_user==bookusername[i-1])
						chosen_user=i-1;
				}
			ImGui::AlignTextToFramePadding(); ImGui::Text(" Users(For returning): "); ImGui::SameLine(); ImGui::SetNextItemWidth(358); ImGui::Combo("##user filter", &chosen_user, bookuser.data(), (int)bookuser.size());
			ImGui::Text("");
			ImGui::SetCursorPosX(420);
	    	if(ImGui::Button("Close", ImVec2(80,40))){
				ImGui::CloseCurrentPopup();
				popup_book = -1;
			}
			ImGui::SameLine(); 
			if(bk.copies_left<bk.copies){
				ImGui::SetCursorPosX(250);
				if(ImGui::Button("Return", ImVec2(80,40)))
					if(MyLibrary.return_book_id(bk.id,bookusername[chosen_user])){
						user_current_page=1;
						chosen_user=0;
					}
			}
			else{
				ImGui::SetCursorPosX(190);
				//ImGui::SetCursorPosY(ImGui::GetWindowContentRegionMax().y - 40);
				ImGui::PushStyleColor(ImGuiCol_Text,IM_COL32(40, 220, 40, 255));
				ImGui::Text("All books have been returned");
				ImGui::PopStyleColor();
			}
			ImGui::SameLine();
			if(bk.copies_left>0){
				ImGui::SetCursorPosX(80);
				if(ImGui::Button("Borrow", ImVec2(80,40)))
					if(MyLibrary.borrow_book_id(bk.id,user_buf,borrow_number_buf+1)){
						user_current_page=1;
						borrow_number_buf=0;
						user_buf="";
					}
			}
			else{
				ImGui::SetCursorPosX(80);
				//ImGui::SetCursorPosY(ImGui::GetWindowContentRegionMax().y - 40);
				ImGui::PushStyleColor(ImGuiCol_Text,IM_COL32(255, 40, 40, 255));
				ImGui::Text("Out of stock");
				ImGui::PopStyleColor();
			}
			ImGui::Text(" ");
			ImGui::Text("      "); ImGui::SameLine();
			ImGui::SliderInt("##NewRate", &tmp_rate, 1, 10);
			ImGui::SameLine();
			if(ImGui::Button("Rate this Book")){
				MyLibrary.rating_book_id(bk.id,tmp_rate);
				ShowToast("Rating updated!",3,1);
				tmp_rate=5;
			} 
			ImGui::Text("");
			ImGui::EndPopup();
		}
		
		double t=ImGui::GetTime(); int info_number=0; // warning sign
		for(auto it=toast_info.begin();it!=toast_info.end();){
			if(t < get<1>(*it)){
				ImGuiIO& io = ImGui::GetIO();
				ImGui::PushStyleColor(ImGuiCol_WindowBg, IM_COL32(0,0,0,128));
				float alpha=(get<1>(*it)-t>0.79?0.79:get<1>(*it)-t)/0.8;
				ImGui::PushStyleColor(ImGuiCol_Text, (get<2>(*it)==2?IM_COL32(255,0,0,(int)(255.0*alpha)):(get<2>(*it)==0?IM_COL32(255,255,0,(int)(255.0*alpha)):IM_COL32(10,255,10,(int)(255.0*alpha))))); //notification type
				ImGui::SetNextWindowBgAlpha(alpha);
				ImGui::SetNextWindowSize(ImVec2(0, 35));
				ImGui::SetNextWindowPos(ImVec2(10, io.DisplaySize.y - 60 - 60*(info_number++)), ImGuiCond_Always);
				string winID="##TOAST"+to_string(info_number);
				ImGui::Begin(winID.c_str(), nullptr, ImGuiWindowFlags_NoDecoration | ImGuiWindowFlags_AlwaysAutoResize | ImGuiWindowFlags_NoFocusOnAppearing | ImGuiWindowFlags_NoMove | ImGuiWindowFlags_NoNav);
				string toast=get<0>(*it);
				ImGui::TextUnformatted(toast.c_str());
				ImGui::End();
				ImGui::PopStyleColor(2);
				it++;
			}
			else
				it=toast_info.erase(it);
		}
		
		ImGui::Render();
		signed w,h; glfwGetFramebufferSize(win,&w,&h);
		glViewport(0,0,w,h);
		glClearColor(0.1f,0.1f,0.1f,1);
		glClear(GL_COLOR_BUFFER_BIT);
		ImGui_ImplOpenGL3_RenderDrawData(ImGui::GetDrawData());
		glfwSwapBuffers(win);
	}
	
	ImGui_ImplOpenGL3_Shutdown();
	ImGui_ImplGlfw_Shutdown();
	ImGui::DestroyContext();
	glfwDestroyWindow(win);
	glfwTerminate();
}
