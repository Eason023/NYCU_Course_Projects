#include <string>
#include <bits/stdc++.h>
#include <algorithm>
#include <ext/pb_ds/assoc_container.hpp>
#include <ext/pb_ds/tree_policy.hpp>
#define int long long
using namespace std;
using namespace __gnu_pbds;

template<class Key, class Val>
using od_map = tree<Key, Val, less<Key>, rb_tree_tag, tree_order_statistics_node_update>;
template<class T>
using od_set = tree<T, null_type, less<T>, rb_tree_tag, tree_order_statistics_node_update>;

string *nullstr=new string("");
int *i=new int(0),*super_tmp=new int(0);

class Basic_task{
protected:
	std::string *name;
	std::string *category;
	bool *completed;
};

class Data:public Basic_task{
private:
	int *order= new int(0);
	bool *importance;
	string *note= new string("");
public:
	static int *cnt;
	static bool *df_ipt,*df_cmplt;
	static string *df_ctg;
	Data(string *a,string *b,int *c,int *d,string *s,int *spe_order=nullptr){
		name=new string(*a);
		if(*b!="-") category=new string(*b);
		else category=new string(*df_ctg);
		if(*c!=-1) importance=new bool(*c);
		else importance=new bool(*df_ipt);
		if(*d!=-1) completed=new bool(*d);
		else completed=new bool(*df_cmplt);
		if((*s).size()!=0) *note=*s;
		if(spe_order!=nullptr) *order=*spe_order;
		else *order=++(*cnt);
	}
	static void set_df(string *b,int *c,int *d){
		*df_ctg=*b,*df_ipt=bool(*c),*df_cmplt=bool(*d);
	}
	void modify(string *a,string *b,int *c,int *d){ //結果沒用到w 
		if(*a!="-")
			*name=*a;
		if(*b!="-")
			*category=*b;
		if(*c!=-1)
			*importance=*c;
		if(*d!=-1)
			*completed=*d;
	}
	void view_data(){
		cout<<"Task: "<<*name<<" | Category: "<<*category<<" | Important: "<<((*importance)?"Yes":"No")<<" | Completed: "<<((*completed)?"Yes":"No");
		if(*note!="")
			cout<<" | Note:"<<*note;
		cout<<'\n';
	}
	tuple<string,string,bool,bool,int> get_data(){
		return make_tuple(*name,*category,*importance,*completed,*order);
	}
	string get_note(){
		return *note;
	}
	~Data(){
		delete name;
		delete category;
		delete importance;
		delete completed;
		delete order;
		delete note;
	}
};

int *Data::cnt=new int(0);
bool *Data::df_ipt=new bool(0);
bool *Data::df_cmplt=new bool(0);
string *Data::df_ctg=new string("My_list");

class MyDB{
private:
	od_map<int,Data*> *order_data;
	map<string,od_set<int> > *name_to_order,*category_to_order;
	od_set<int> *completed_task,*uncompleted_task,*important_task,*unimportant_task;
	deque<tuple<int,int,Data*,Data*> > *undo_stk,*redo_stk; //op_type data_order Data_address
	void nodata(){
		cout<<"No such task.\n";
	}
	void clear_redo(){
		auto *it= new auto(redo_stk->begin());
		for(;(*it)!=redo_stk->end();(*it)++){
			if(get<0>(*(*it))==2) //!!! Important !!!
				delete get<2>(*(*it));
			else if(get<0>(*(*it))==4)
				delete get<3>(*(*it));
		}
		redo_stk->clear();
		delete it;
	}
	void add_by_addr(Data* tmp){
		order_data->insert(make_pair(get<4>(tmp->get_data()),tmp));
		(*name_to_order)[get<0>(tmp->get_data())].insert(get<4>(tmp->get_data()));
		(*category_to_order)[get<1>(tmp->get_data())].insert(get<4>(tmp->get_data()));
		if(get<3>(tmp->get_data())==1)
			completed_task->insert(get<4>(tmp->get_data()));
		else
			uncompleted_task->insert(get<4>(tmp->get_data()));
		if(get<2>(tmp->get_data())==1)
			important_task->insert(get<4>(tmp->get_data()));
		else
			unimportant_task->insert(get<4>(tmp->get_data()));
	}
	void del_by_addr(Data* tmp){
		order_data->erase(get<4>(tmp->get_data()));
		(*name_to_order)[get<0>(tmp->get_data())].erase(get<4>(tmp->get_data()));
		(*category_to_order)[get<1>(tmp->get_data())].erase(get<4>(tmp->get_data()));
		if(get<3>(tmp->get_data())==1)
			completed_task->erase(get<4>(tmp->get_data()));
		else
			uncompleted_task->erase(get<4>(tmp->get_data()));
		if(get<2>(tmp->get_data())==1)
			important_task->erase(get<4>(tmp->get_data()));
		else
			unimportant_task->erase(get<4>(tmp->get_data()));
	}

public:
	MyDB(){
		order_data=new od_map<int,Data*>();
		name_to_order=new map<string,od_set<int> >(), category_to_order=new map<string,od_set<int> >();
		completed_task=new od_set<int>(),uncompleted_task=new od_set<int>(),important_task=new od_set<int>(),unimportant_task=new od_set<int>();
		undo_stk=new deque<tuple<int,int,Data*,Data*> >(),redo_stk=new deque<tuple<int,int,Data*,Data*> >();
	}
	~MyDB(){
		clear_redo(); auto *it=new auto(order_data->begin());
		while(*it!=order_data->end()){
			delete (*it)->second; (*it)++;
		}
		delete it;
		delete order_data; delete name_to_order; delete category_to_order; delete completed_task; delete uncompleted_task;
		delete important_task; delete unimportant_task; delete undo_stk; delete redo_stk;
	}
	void add(string *name,string *ctg,int *ipt,int *complt,string *note=nullptr){ //for cmd only
		Data* tmp=new Data(name,ctg,ipt,complt,(note==nullptr?nullstr:note));
		//cout<<"node: ";//<<get<0>(nd)<<get<1>(nd)<<get<2>(nd)<<get<3>(nd)<<get<4>(nd)<<'\n';
		*name=get<0>(tmp->get_data()),*ctg=get<1>(tmp->get_data()),*ipt=get<2>(tmp->get_data()),*complt=get<3>(tmp->get_data());
		order_data->insert(make_pair(get<4>(tmp->get_data()),tmp));
		(*name_to_order)[*name].insert(get<4>(tmp->get_data()));
		(*category_to_order)[*ctg].insert(get<4>(tmp->get_data()));
		if(get<3>(tmp->get_data())==1)
			completed_task->insert(get<4>(tmp->get_data()));
		else
			uncompleted_task->insert(get<4>(tmp->get_data()));
		if(get<2>(tmp->get_data())==1)
			important_task->insert(get<4>(tmp->get_data()));
		else
			unimportant_task->insert(get<4>(tmp->get_data()));
		clear_redo();
		undo_stk->push_back(make_tuple(1,get<4>(tmp->get_data()),tmp,nullptr));
	}
	void undo(){
		if(undo_stk->empty()){
			cout<<"Can't undo, there is no operation record.\n";
			return;
		}
		Data* tmp=get<2>(undo_stk->back());
		if(get<0>(undo_stk->back())==1){
			del_by_addr(tmp);
			redo_stk->push_back(make_tuple(2,get<1>(undo_stk->back()),tmp,nullptr));
			cout<<"Task: "<<get<0>(tmp->get_data())<<" in "<<get<1>(tmp->get_data())<<" has been deleted from list.\n";
		}
		else if(get<0>(undo_stk->back())==2){
			add_by_addr(tmp);
			redo_stk->push_back(make_tuple(1,get<1>(undo_stk->back()),tmp,nullptr));
			cout<<"Task: "<<get<0>(tmp->get_data())<<" in "<<get<1>(tmp->get_data())<<" has been re-added to list.\n";
		}
		else if(get<0>(undo_stk->back())==3){
			int *tmp2=new int(get<1>(undo_stk->back()));
			undo_stk->pop_back();
			if(*tmp2==0){
				cout<<"Previous operation didn\'t change any task in the list.\n";
				delete tmp2;
				return;
			}
			for((*i)=0;(*i)<*tmp2;(*i)++)
				undo();
			redo_stk->push_back(make_tuple(3,*tmp2,nullptr,nullptr));
			delete tmp2;
			return;
		}
		else if(get<0>(undo_stk->back())==4){
			Data* tmpback=get<3>(undo_stk->back());
			del_by_addr(tmp);
			add_by_addr(tmpback);
			redo_stk->push_back(make_tuple(4,get<1>(undo_stk->back()),tmpback,tmp));
			cout<<"Task: "<<get<0>(tmp->get_data())<<" in "<<get<1>(tmp->get_data())<<" has been modified back.\n";
		}
		undo_stk->pop_back();
	}
	void redo(){
		if(redo_stk->empty()){
			cout<<"Can't redo, there is no operation record.\n";
			return;
		}
		Data* tmp=get<2>(redo_stk->back());
		if(get<0>(redo_stk->back())==1){
			del_by_addr(tmp);
			undo_stk->push_back(make_tuple(2,get<1>(redo_stk->back()),tmp,nullptr));
			cout<<"Task: "<<get<0>(tmp->get_data())<<" in "<<get<1>(tmp->get_data())<<" has been deleted from list.\n";
		}
		else if(get<0>(redo_stk->back())==2){
			add_by_addr(tmp);
			undo_stk->push_back(make_tuple(1,get<1>(redo_stk->back()),tmp,nullptr));
			cout<<"Task: "<<get<0>(tmp->get_data())<<" in "<<get<1>(tmp->get_data())<<" has been re-added to list.\n";
		}
		else if(get<0>(redo_stk->back())==3){
			int *tmp2=new int(get<1>(redo_stk->back()));
			redo_stk->pop_back();
			if(*tmp2==0){
				cout<<"The next operation doesn\'t change any task in the list.\n";
				delete tmp2;
				return;
			}
			for((*i)=0;(*i)<*tmp2;(*i)++)
				redo();
			undo_stk->push_back(make_tuple(3,*tmp2,nullptr,nullptr));
			delete tmp;
			return;
		}
		else if(get<0>(redo_stk->back())==4){
			Data* tmpback=get<3>(redo_stk->back());
			del_by_addr(tmp);
			add_by_addr(tmpback);
			undo_stk->push_back(make_tuple(4,get<1>(redo_stk->back()),tmpback,tmp));
			cout<<"Task: "<<get<0>(tmp->get_data())<<" in "<<get<1>(tmp->get_data())<<" has been modified back.\n";
		}
		redo_stk->pop_back();
	}
	void view_all(string *op){
		bool *out_tmp=new bool(0);
		if(*op=="category"){
			(*i)=1;
			auto *it=new auto(category_to_order->begin());
			for(;(*it)!=category_to_order->end();(*it)++){
				auto *it2=new auto(((*it)->second).begin());
				for(;(*it2)!=((*it)->second).end();(*it2)++){
					Data* p=(order_data->find(*(*it2))->second);
					cout<<(*i)<<"-> ";
					p->view_data(); *out_tmp=1;
					(*i)++;
				}
				delete it2;
			}
			delete it;
		}
		else if(*op=="importance"||*op=="completed"){
			auto *task_set=(*op=="importance"?important_task:completed_task),*task_set2=(*op=="importance"?unimportant_task:uncompleted_task);
			(*i)=1;
			auto *it=new auto(task_set->begin());
			for(;(*it)!=task_set->end();(*it)++){
				Data* p=(order_data->find(*(*it))->second);
				cout<<(*i)<<"-> ";
				p->view_data(); *out_tmp=1;
				(*i)++;
			}
			delete it;
			auto *it2=new auto(task_set2->begin());
			for(;(*it2)!=task_set2->end();(*it2)++){
				Data* p=(order_data->find(*(*it2))->second);
				cout<<(*i)<<"-> ";
				p->view_data(); *out_tmp=1;
				(*i)++;
			}
			delete it2;
		}
		else if(*op==""||*op=="all"){
			(*i)=1;
			auto *it=new auto(order_data->begin());
			for(;(*it)!=order_data->end();(*it)++){
				cout<<(*i)<<"-> ";
				(*(*it)).second->view_data(); *out_tmp=1;
				(*i)++;
			}
			delete it;
		}
		else{
			cout<<"Invalid argument \""<<*op<<"\", please try again.\n";
			delete out_tmp;
			return;
		}
		if(*out_tmp==0)
			cout<<"No data.\n";
		delete out_tmp;
	}
	void view_task(string *op){
		(*i)=1; bool *out_tmp=new bool(0);
		auto *it=new auto(name_to_order->find(*op));
		if((*it)!=name_to_order->end()){
			auto *it2=new auto(((*it)->second).begin());
			for(;(*it2)!=((*it)->second).end();(*it2)++){
				Data* p=(order_data->find(*(*it2))->second);
				cout<<(*i)<<"-> ";
				p->view_data(); *out_tmp=1;
				(*i)++;
			}
			delete it2;
		}
		else
			cout<<"No such task. ";
		if(*out_tmp==0)
			cout<<"No data.\n";
		delete out_tmp;
		delete it;
	}
	void view_category(string *op){
		(*i)=1; bool *out_tmp=new bool(0);
		auto *it=new auto(category_to_order->find(*op));
		if((*it)!=category_to_order->end()){
			auto *it2=new auto(((*it)->second).begin());
			for(;(*it2)!=((*it)->second).end();(*it2)++){
				Data* p=(order_data->find(*(*it2))->second);
				cout<<(*i)<<"-> ";
				p->view_data(); *out_tmp=1;
				(*i)++;
			}
			delete it2;
		}
		else
			cout<<"No such category. ";
		if(*out_tmp==0)
			cout<<"No data.\n";
		delete out_tmp;
		delete it;
	}
	void view_importance(int *yes_no){
		(*i)=1; bool *out_tmp=new bool(0);
		od_set<int> *task_set=(*yes_no?important_task:unimportant_task);
		auto *it=new auto((*task_set).begin());
		for(;(*it)!=(*task_set).end();(*it)++){
			Data* p=(order_data->find(*(*it))->second);
			cout<<(*i)<<"-> ";
			p->view_data(); *out_tmp=1;
			(*i)++;
		}
		if(*out_tmp==0)
			cout<<"No data.\n";
		delete out_tmp;
		delete it;
	}
	void view_completed(int *yes_no){
		(*i)=1; bool *out_tmp=new bool(0);
		od_set<int> *task_set=(*yes_no?completed_task:uncompleted_task);
		auto *it=new auto((*task_set).begin());
		for(;(*it)!=(*task_set).end();(*it)++){
			Data* p=(order_data->find(*(*it))->second);
			cout<<(*i)<<"-> ";
			p->view_data(); *out_tmp=1;
			(*i)++;
		}
		if(*out_tmp==0)
			cout<<"No data.\n";
		delete out_tmp;
		delete it;
	}
	void view_order(int *order){
		auto *it=new auto(order_data->find_by_order(*order-1)); bool *out_tmp=new bool(0);
		if(*it!=order_data->end()){
			Data* p=((*it)->second);
			cout<<*order<<"-> "; *out_tmp=1;
			p->view_data();
		}
		else
			cout<<"No such task. ";
		if(*out_tmp==0)
			cout<<"No data.\n";
		delete out_tmp;
		delete it;
	}
	void del_by_type(int *type,string *attribute,int *order){
		//cout<<"Try del: "<<*type<<' '<<*attribute<<'\n';
		if(*type==1||*type==2){ //by task name or category
			auto *task_map=(*type==1?name_to_order:category_to_order);
			if(task_map->find(*attribute)==task_map->end()||(*order) > ((task_map->find(*attribute))->second).size()){
				//cout<<' '<<((*order) > (task_map->find(*attribute)->second.size()))<<"here\n";
				//cout<<(-1>2);
				nodata();
				return;
			}
			auto *it=new auto(task_map->find(*attribute));
			if(*order!=0){
				*super_tmp=*(((*it)->second).find_by_order(*order-1));
				Data* tmp=order_data->find(*super_tmp)->second;
				del_by_addr(tmp);
				clear_redo();
				undo_stk->push_back(make_tuple(2,get<4>(tmp->get_data()),tmp,nullptr));
			}
			else{
				auto *it2=new auto(((*it)->second).begin());
				clear_redo();
				vector<Data*> *v=new vector<Data*>();
				for(;(*it2)!=((*it)->second).end();(*it2)++){
					//cout<<"it2"<<' '<<*(*it2)<<'\n';
					Data* tmp=(order_data->find(*(*it2))->second);
					v->push_back(tmp);
					undo_stk->push_back(make_tuple(2,get<4>(tmp->get_data()),tmp,nullptr)); 
				}
				delete it2;
				for((*i)=0;(*i) < v->size();(*i)++)
					del_by_addr((*v)[(*i)]);
				undo_stk->push_back(make_tuple(3,v->size(),nullptr,nullptr)); //3 means multi-op
				delete v;
			}
			delete it;
		}
		else if(*type==3||*type==4){ //by importance or completed
			auto *task_set=(*type==3?(*attribute=="1"?important_task:unimportant_task):(*attribute=="1"?completed_task:uncompleted_task));
			if(*order > task_set->size()){
				nodata();
				return;
			}
			if(*order!=0){
				Data* tmp=order_data->find(*(task_set->find_by_order(*order-1)))->second;
				del_by_addr(tmp);
				clear_redo();
				undo_stk->push_back(make_tuple(2,get<4>(tmp->get_data()),tmp,nullptr));
			}
			else{
				auto *it2=new auto(task_set->begin());
				clear_redo();
				vector<Data*> *v=new vector<Data*>();
				for(;(*it2)!=task_set->end();(*it2)++){
					Data* tmp=(order_data->find(*(*it2))->second);
					v->push_back(tmp);
					undo_stk->push_back(make_tuple(2,get<4>(tmp->get_data()),tmp,nullptr));
				}
				delete it2;
				for((*i)=0;(*i) < v->size();(*i)++)
					del_by_addr((*v)[(*i)]);
				undo_stk->push_back(make_tuple(3,v->size(),nullptr,nullptr));
				delete v;
			}
		}
		cout<<"This is the list after delete operation:\n";
		view_all(nullstr);
	}
	void modify_by_type(int *type,string *attribute,int *order,string *str,string *str2,string *str3,string *str4){
		//cout<<"Try del: "<<*type<<' '<<*attribute<<'\n';
		if(*type==1||*type==2){ //by task name or category
			auto *task_map=(*type==1?name_to_order:category_to_order);
			if(task_map->find(*attribute)==task_map->end()||(*order) > ((task_map->find(*attribute))->second).size()){
				//cout<<' '<<((*order) > (task_map->find(*attribute)->second.size()))<<"here\n";
				//cout<<(-1>2);
				nodata();
				return;
			}
			auto *it=new auto(task_map->find(*attribute));
			if(*order!=0){
				*super_tmp=*(((*it)->second).find_by_order(*order-1));
				Data* tmp=order_data->find(*super_tmp)->second;
				if(*str=="-") *str=get<0>(tmp->get_data());
				if(*str2=="-") *str2=get<1>(tmp->get_data());
				if(*str3=="-") *str3=(get<2>(tmp->get_data())?"1":"0");
				if(*str4=="-") *str4=(get<3>(tmp->get_data())?"1":"0");
				string *snt=new string(tmp->get_note()); int *od=new int(get<4>(tmp->get_data()));
				int *tt1=new int(*str3=="1"?1:0),*tt2=new int(*str4=="1"?1:0);
				Data* tmp2=new Data(str,str2,tt1,tt2,snt,od);
				delete snt; delete od; delete tt1; delete tt2;
				del_by_addr(tmp);
				add_by_addr(tmp2);
				clear_redo();
				undo_stk->push_back(make_tuple(4,get<4>(tmp2->get_data()),tmp2,tmp)); // 4 id new_addr old_addr
			}
			else{
				auto *it2=new auto(((*it)->second).begin());
				clear_redo();
				vector<Data*> *v=new vector<Data*>();
				for(;(*it2)!=((*it)->second).end();(*it2)++){
					//cout<<"it2"<<' '<<*(*it2)<<'\n';
					Data* tmp=(order_data->find(*(*it2))->second);
					v->push_back(tmp);
				}
				delete it2;
				string *st=new string(""),*st2=new string(""),*st3=new string(""),*st4=new string("");
				for((*i)=0;(*i) < v->size();(*i)++){
					Data* tmp=(*v)[(*i)];
					if(*str=="-") *st=get<0>(tmp->get_data());
					else *st=*str;
					if(*str2=="-") *st2=get<1>(tmp->get_data());
					else *st2=*str2;
					if(*str3=="-") *st3=(get<2>(tmp->get_data())?"1":"0");
					else *st3=*str3;
					if(*str4=="-") *st4=(get<3>(tmp->get_data())?"1":"0");
					else *st4=*str4;
					string *snt=new string(tmp->get_note()); int *od=new int(get<4>(tmp->get_data()));
					int *tt1=new int(*st3=="1"?1:0),*tt2=new int(*st4=="1"?1:0);
					Data* tmp2=new Data(st,st2,tt1,tt2,snt,od);
					delete snt; delete od; delete tt1; delete tt2;
					del_by_addr(tmp);
					add_by_addr(tmp2);
					undo_stk->push_back(make_tuple(4,get<4>(tmp2->get_data()),tmp2,tmp));
				}
				undo_stk->push_back(make_tuple(3,v->size(),nullptr,nullptr)); //3 means multi-op
				delete v; delete st; delete st2; delete st3; delete st4;
			}
			delete it;
		}
		else if(*type==3||*type==4){ //by importance or completed
			auto *task_set=(*type==3?(*attribute=="1"?important_task:unimportant_task):(*attribute=="1"?completed_task:uncompleted_task));
			if(*order > task_set->size()){
				nodata();
				return;
			}
			if(*order!=0){
				Data* tmp=order_data->find(*(task_set->find_by_order(*order-1)))->second;
				if(*str=="-") *str=get<0>(tmp->get_data());
				if(*str2=="-") *str2=get<1>(tmp->get_data());
				if(*str3=="-") *str3=(get<2>(tmp->get_data())?"1":"0");
				if(*str4=="-") *str4=(get<3>(tmp->get_data())?"1":"0");
				string *snt=new string(tmp->get_note()); int *od=new int(get<4>(tmp->get_data()));
				int *tt1=new int(*str3=="1"?1:0),*tt2=new int(*str4=="1"?1:0);
				Data* tmp2=new Data(str,str2,tt1,tt2,snt,od);
				delete snt; delete od; delete tt1; delete tt2;
				del_by_addr(tmp);
				add_by_addr(tmp2);
				clear_redo();
				undo_stk->push_back(make_tuple(4,get<4>(tmp2->get_data()),tmp2,tmp));
			}
			else{
				auto *it2=new auto(task_set->begin());
				clear_redo();
				vector<Data*> *v=new vector<Data*>();
				for(;(*it2)!=task_set->end();(*it2)++){
					Data* tmp=(order_data->find(*(*it2))->second);
					v->push_back(tmp);
				}
				delete it2;
				string *st=new string(""),*st2=new string(""),*st3=new string(""),*st4=new string("");
				for((*i)=0;(*i) < v->size();(*i)++){
					//cout<<"hi ";
					Data* tmp=(*v)[(*i)];
					//tmp->view_data();
					if(*str=="-") *st=get<0>(tmp->get_data());
					else *st=*str;
					if(*str2=="-") *st2=get<1>(tmp->get_data());
					else *st2=*str2;
					if(*str3=="-") *st3=(get<2>(tmp->get_data())?"1":"0");
					else *st3=*str3;
					if(*str4=="-") *st4=(get<3>(tmp->get_data())?"1":"0");
					else *st4=*str4;
					//cout<<*str<<' '<<*str2<<' '<<*str3<<' '<<*str4<<'\n';
					string *snt=new string(tmp->get_note()); int *od=new int(get<4>(tmp->get_data()));
					int *tt1=new int(*st3=="1"?1:0),*tt2=new int(*st4=="1"?1:0);
					Data* tmp2=new Data(st,st2,tt1,tt2,snt,od);
					delete snt; delete od; delete tt1; delete tt2;
					del_by_addr(tmp);
					add_by_addr(tmp2);
					undo_stk->push_back(make_tuple(4,get<4>(tmp2->get_data()),tmp2,tmp));
				}
				undo_stk->push_back(make_tuple(3,v->size(),nullptr,nullptr));
				delete v; delete st; delete st2; delete st3; delete st4;
			}
		}
		cout<<"This is the list after modify operation:\n";
		view_all(nullstr);
	}
	void show_log(){
		if((*undo_stk).empty()){
			cout<<"No log data.\n";
			return;
		}
		auto *it= new auto(undo_stk->begin());
		for(;(*it)!=undo_stk->end();(*it)++){
			if(get<0>(*(*it))==1){
				cout<<"    Add  : ";
				get<2>(*(*it))->view_data();
			}
			else if(get<0>(*(*it))==2){
				cout<<"  Delete : ";
				get<2>(*(*it))->view_data();
			}
			else if(get<0>(*(*it))==4){
				cout<<"  Modify : ";
				get<3>(*(*it))->view_data();
				cout<<"Modified : ";
				get<2>(*(*it))->view_data();
			}
		}
		delete it;
	}
	void export_data(){
		if(order_data->begin()==order_data->end()){
			cout<<"No data to export.\n";
			return;
		}
		try{
			ofstream file("To-Do list.csv");
			auto *ptr=new auto(order_data->begin());
			file<<"Task, Category, Importance, Completed, Note"<<'\n';
			while(*ptr!=order_data->end()){
				Data *tmp=(*ptr)->second;
				file<<get<0>(tmp->get_data())<<','<<get<1>(tmp->get_data())<<','<<(get<2>(tmp->get_data())?"Yes":"No")<<','<<(get<3>(tmp->get_data())?"Yes":"No")<<','<<'\"'<<(tmp->get_note())<<'\"'<<'\n';
				(*ptr)++;
			}
			delete ptr;
			file.close();
			cout<<"To-do list data has been exported to current folder successfully.\n";
		}catch(...){
			cout<<"Unexpected issue occurred as trying to export file. Please make sure no other application is using the file.\n";
		}
	}
	void show_stats(){
		if(order_data->begin()==order_data->end()){
			cout<<"No data. No statistical information.\n";
			return;
		}
		cout<<"Total tasks: "<<order_data->size()<<'\n';
		cout<<"Completed tasks: "<<completed_task->size()<<" Uncompleted tasks: "<<uncompleted_task->size()<<'\n';
		cout<<"Completion Rate: "<<fixed<<setprecision(2)<<(double)(completed_task->size())/((double)(completed_task->size())+(double)(uncompleted_task->size()))*100.0<<"%"<<'\n';
		cout<<"Category info:\n";
		auto *ptr=new auto(category_to_order->begin()); *super_tmp=0;
		vector<pair<string,int> > *v=new vector<pair<string,int> >();
		while(*ptr!=category_to_order->end()){
			if((*ptr)->second.size()>0){
				*super_tmp=max(*super_tmp,(int)((*ptr)->first.length()));
				(*v).push_back(make_pair((*ptr)->first,(*ptr)->second.size()));
			}
			(*ptr)++;
		}
		auto *it=new auto(v->begin());
		while(*it!=v->end()){
			cout<<"    "<<(*it)->first;
			for((*i)=(*it)->first.length();(*i)<*super_tmp;(*i)++)
				cout<<' ';
			cout<<" : "<<(*it)->second<<'\n';
			(*it)++;
		}
		delete ptr; delete v; delete it;
	}
};


void strlower(string *str){
	int *tmp=new int(0);
	for(*tmp=0;*tmp<str->size();(*tmp)++)
		if('A'<=(*str)[*tmp]&&(*str)[*tmp]<='Z')
			(*str)[*tmp]='a'+(*str)[*tmp]-'A';
	delete tmp;
}


signed main(){
	//ios::sync_with_stdio(0);
	MyDB *DB=new MyDB();
	//string a="AaB13C"; strlower(&a); cout<<a<<'\n';
	cout<<"OOPDS-HW1 Author:113550153\n";
	cout<<"Simple To-Do Task list here, welcome!\n";
	string *op=new string("");
	while(getline(cin,*op)){
		stringstream *ss= new stringstream(*op); *ss>>*op;
		string *name=new string(""),*ctg=new string(""),*note=new string(""); int *ipt=new int(0),*complt=new int(0);
		strlower(op);
		if(*op=="undo")
			DB->undo();
		else if(*op=="redo")
			DB->redo();
		else if(*op=="add"){
			*ss>>*name>>*ctg;
			if(*name=="-"){
				cout<<"Invalid name value \""<<*name<<"\", please try again.\n"; delete ctg; delete name; delete ipt; delete note; delete complt; delete ss;
				continue;
			}
			*ss>>*op; strlower(op);
			if(*op=="y"||*op=="yes") *ipt=1;
			else if(*op=="n"||*op=="no") *ipt=0;
			else if(*op=="-") *ipt=-1;
			else{
				cout<<"Invalid value \""<<*op<<"\", please try again.\n"; delete ctg; delete name; delete ipt; delete note; delete complt; delete ss;
				continue;
			}
			*ss>>*op; strlower(op);
			if(*op=="y"||*op=="yes") *complt=1;
			else if(*op=="n"||*op=="no") *complt=0;
			else if(*op=="-") *complt=-1;
			else{
				cout<<"Invalid value \""<<*op<<"\", please try again.\n"; delete ctg; delete name; delete ipt; delete note; delete complt; delete ss;
				continue;
			}
			getline(*ss,*note); //cout<<note<<'\n';
			DB->add(name,ctg,ipt,complt,note);
		}
		else if(*op=="view"){
			*ss>>*op;
			if(*op=="all"){
				*ss>>*op;
				DB->view_all(op);
			}
			else{
				if(*op=="task"){
					if(*ss>>*op)
						DB->view_task(op);
					else{
						cout<<"No argument error, please try again.\n"; delete ctg; delete name; delete ipt; delete note; delete complt; delete ss;
						continue;
					}
				}
				else if(*op=="category"){
					if(*ss>>*op)
						DB->view_category(op);
					else{
						cout<<"No argument error, please try again.\n"; delete ctg; delete name; delete ipt; delete note; delete complt; delete ss;
						continue;
					}
				}
				else if(*op=="importance"){
					if(*ss>>*op){
						strlower(op);
						if(*op=="y"||*op=="yes") *ipt=1;
						else if(*op=="n"||*op=="no") *ipt=0;
						else{
							cout<<"Argument error, please try again.\n"; delete ctg; delete name; delete ipt; delete note; delete complt; delete ss;
							continue;
						}
						DB->view_importance(ipt);
					}
					else{
						cout<<"No argument error, please try again.\n"; delete ctg; delete name; delete ipt; delete note; delete complt; delete ss;
						continue;
					}
				}
				else if(*op=="completed"){
					if(*ss>>*op){
						strlower(op); 
						if(*op=="y"||*op=="yes") *ipt=1;
						else if(*op=="n"||*op=="no") *ipt=0;
						else{
							cout<<"Argument error, please try again.\n"; delete ctg; delete name; delete ipt; delete note; delete complt; delete ss;
							continue;
						}
						DB->view_completed(ipt);
					}
					else{
						cout<<"No argument error, please try again.\n"; delete ctg; delete name; delete ipt; delete note; delete complt; delete ss;
						continue;
					}
				}
				else if(*op=="order"){
					if(*ss>>*op){
						try{
							int *tmpodr=new int(stoll(*op));
							DB->view_order(tmpodr);
							delete tmpodr;
						}catch(...){
							cout<<"Argument error, please try again.\n";
						}
					}
					else{
						cout<<"No argument error, please try again.\n"; delete ctg; delete name; delete ipt; delete note; delete complt; delete ss;
						continue;
					}
				}
				else
					cout<<"Require option value, please try again.\n";
			}
		}
		else if(*op=="delete"){
			*ss>>*op; strlower(op);
			if(*op=="all"){
				*ss>>*op; strlower(op);
				*ipt=0;
				if(*op=="task"||*op=="category"){
					*ss>>*name;
					*complt=(*op=="task"?1:2);
					DB->del_by_type(complt,name,ipt);
				}
				else if(*op=="importance"||*op=="completed"){
					*complt=(*op=="importance"?3:4);
					*ss>>*ctg; strlower(ctg);
					if(*ctg=="y"||*ctg=="yes") *ctg="1";
					else if(*ctg=="n"||*ctg=="no") *ctg="0";
					else{
						cout<<"Invalid value \""<<*ctg<<"\", please try again.\n"; delete ctg; delete name; delete ipt; delete note; delete complt; delete ss;
						continue;
					}
					DB->del_by_type(complt,ctg,ipt);
				}
				else{
					cout<<"Invalid option \""<<*op<<"\", please try again.\n"; delete ctg; delete name; delete ipt; delete note; delete complt; delete ss;
					continue;
				}
			}
			else if(*op=="task"||*op=="category"||*op=="importance"||*op=="completed"){
				if(*op=="task"||*op=="category"){
					*complt=(*op=="task"?1:2);
					int *tmpodr=new int(0);
					try{
						*ss>>*name>>*ctg;
						*tmpodr=(stoll(*ctg));
						if(*tmpodr<=0) throw runtime_error("fail");
						DB->del_by_type(complt,name,tmpodr);
					}catch(...){
						cout<<"Argument error, please try again.\n";
					}
					delete tmpodr;
				}
				else if(*op=="importance"||*op=="completed"){
					*complt=(*op=="importance"?3:4);
					*ss>>*ctg; strlower(ctg);
					if(*ctg=="y"||*ctg=="yes") *ctg="1";
					else if(*ctg=="n"||*ctg=="no") *ctg="0";
					else{
						cout<<"Invalid value \""<<*ctg<<"\", please try again.\n"; delete ctg; delete name; delete ipt; delete note; delete complt; delete ss;
						continue;
					}
					int *tmpodr=new int(0);
					try{
						*ss>>*name;
						*tmpodr=(stoll(*name));
						if(*tmpodr<=0) throw runtime_error("fail");
						DB->del_by_type(complt,ctg,tmpodr);
					}catch(...){
						cout<<"Argument error, please try again.\n";
					}
					delete tmpodr;
				}
			}
			else{
				cout<<"Invalid option \""<<*op<<"\", please try again.\n"; delete ctg; delete name; delete ipt; delete note; delete complt; delete ss;
				continue;
			}
		}
		else if(*op=="modify"){
			*ss>>*op; strlower(op);
			if(*op=="all"){
				*ss>>*op; strlower(op);
				*ipt=0;
				if(*op=="task"||*op=="category"){
					*ss>>*name;
					*complt=(*op=="task"?1:2);
					string *str=new string(""),*str2=new string(""),*str3=new string(""),*str4=new string("");
					try{
						*ss>>*str>>*str2>>*str3>>*str4; strlower(str3); strlower(str4);
						//int *tmpodr=new int(stoll(*name));
						if(*str3=="y"||*str3=="yes") *str3="1";
						else if(*str3=="n"||*str3=="no") *str3="0";
						else if(*str3!="-"){
							cout<<"Invalid value \""<<*str3<<"\". ";
							throw runtime_error("fail");
						}
						if(*str4=="y"||*str4=="yes") *str4="1";
						else if(*str4=="n"||*str4=="no") *str4="0";
						else if(*str4!="-"){
							cout<<"Invalid value \""<<*str4<<"\". ";
							throw runtime_error("fail");
						}
						DB->modify_by_type(complt,name,ipt,str,str2,str3,str4);
						//delete tmpodr;
					}catch(...){
						cout<<"Argument error, please try again.\n";
					}
					delete str; delete str2; delete str3; delete str4;
				}
				else if(*op=="importance"||*op=="completed"){
					*complt=(*op=="importance"?3:4);
					*ss>>*ctg; strlower(ctg);
					if(*ctg=="y"||*ctg=="yes") *ctg="1";
					else if(*ctg=="n"||*ctg=="no") *ctg="0";
					else{
						cout<<"Invalid value \""<<*ctg<<"\", please try again.\n"; delete ctg; delete name; delete ipt; delete note; delete complt; delete ss;
						continue;
					}
					string *str=new string(""),*str2=new string(""),*str3=new string(""),*str4=new string("");
					try{
						*ss>>*str>>*str2>>*str3>>*str4; strlower(str3); strlower(str4);
						//int *tmpodr=new int(stoll(*name));
						if(*str3=="y"||*str3=="yes") *str3="1";
						else if(*str3=="n"||*str3=="no") *str3="0";
						else if(*str3!="-"){
							cout<<"Invalid value \""<<*str3<<"\". ";
							throw runtime_error("fail");
						}
						if(*str4=="y"||*str4=="yes") *str4="1";
						else if(*str4=="n"||*str4=="no") *str4="0";
						else if(*str4!="-"){
							cout<<"Invalid value \""<<*str4<<"\". ";
							throw runtime_error("fail");
						}
						DB->modify_by_type(complt,ctg,ipt,str,str2,str3,str4);
						//delete tmpodr;
					}catch(...){
						cout<<"Argument error, please try again.\n";
					}
					delete str; delete str2; delete str3; delete str4;
				}
				else{
					cout<<"Invalid option \""<<*op<<"\", please try again.\n";
				}
			}
			else if(*op=="task"||*op=="category"||*op=="importance"||*op=="completed"){
				if(*op=="task"||*op=="category"){
					*complt=(*op=="task"?1:2);
					string *str=new string(""),*str2=new string(""),*str3=new string(""),*str4=new string("");
					int *tmpodr=new int(0);
					try{
						*ss>>*name>>*ctg;
						*tmpodr=(stoll(*ctg));
						if(*tmpodr<=0) throw runtime_error("fail");
						*ss>>*str>>*str2>>*str3>>*str4; strlower(str3); strlower(str4);
						if(*str3=="y"||*str3=="yes") *str3="1";
						else if(*str3=="n"||*str3=="no") *str3="0";
						else if(*str3!="-"){
							cout<<"Invalid value \""<<*str3<<"\". ";
							throw runtime_error("fail");
						}
						if(*str4=="y"||*str4=="yes") *str4="1";
						else if(*str4=="n"||*str4=="no") *str4="0";
						else if(*str4!="-"){
							cout<<"Invalid value \""<<*str4<<"\". ";
							throw runtime_error("fail");
						}
						DB->modify_by_type(complt,name,tmpodr,str,str2,str3,str4);
					}catch(...){
						cout<<"Argument error, please try again.\n";
					}
					delete tmpodr;
					delete str; delete str2; delete str3; delete str4;
				}
				else if(*op=="importance"||*op=="completed"){
					*complt=(*op=="importance"?3:4);
					*ss>>*ctg; strlower(ctg);
					if(*ctg=="y"||*ctg=="yes") *ctg="1";
					else if(*ctg=="n"||*ctg=="no") *ctg="0";
					else{
						cout<<"Invalid value \""<<*ctg<<"\", please try again.\n"; delete ctg; delete name; delete ipt; delete note; delete complt; delete ss;
						continue;
					}
					int *tmpodr=new int(0);
					string *str=new string(""),*str2=new string(""),*str3=new string(""),*str4=new string("");
					try{
						*ss>>*name;
						*tmpodr=(stoll(*name));
						if(*tmpodr<=0) throw runtime_error("fail");
						*ss>>*str>>*str2>>*str3>>*str4; strlower(str3); strlower(str4);
						//int *tmpodr=new int(stoll(*name));
						if(*str3=="y"||*str3=="yes") *str3="1";
						else if(*str3=="n"||*str3=="no") *str3="0";
						else if(*str3!="-"){
							cout<<"Invalid value \""<<*str3<<"\". ";
							throw runtime_error("fail");
						}
						if(*str4=="y"||*str4=="yes") *str4="1";
						else if(*str4=="n"||*str4=="no") *str4="0";
						else if(*str4!="-"){
							cout<<"Invalid value \""<<*str4<<"\". ";
							throw runtime_error("fail");
						}
						DB->modify_by_type(complt,ctg,tmpodr,str,str2,str3,str4);
						//delete tmpodr;
					}catch(...){
						cout<<"Argument error, please try again.\n";
					}
					delete str; delete str2; delete str3; delete str4;
					delete tmpodr;
				}
			}
			else{
				cout<<"Invalid option \""<<*op<<"\", please try again.\n";
			}
		}
		else if(*op=="show_log"){
			DB->show_log();
		}
		else if(*op=="set_default"){
			*ss>>*ctg;
			*ss>>*op; strlower(op);
			if(*op=="y"||*op=="yes") *ipt=1;
			else if(*op=="n"||*op=="no") *ipt=0;
			else{
				cout<<"Invalid value \""<<*op<<"\", please try again.\n"; delete ctg; delete name; delete ipt; delete note; delete complt; delete ss;
				continue;
			}
			*ss>>*op; strlower(op);
			if(*op=="y"||*op=="yes") *complt=1;
			else if(*op=="n"||*op=="no") *complt=0;
			else{
				cout<<"Invalid value \""<<*op<<"\", please try again.\n"; delete ctg; delete name; delete ipt; delete note; delete complt; delete ss;
				continue;
			}
			Data::set_df(ctg,ipt,complt);
			cout<<"Default class has been changed!\n";
		}
		else if(*op=="export"){
			DB->export_data();
		}
		else if(*op=="show_statistics"){
			DB->show_stats();
		}
		else if(*op=="exit"){
			cout<<"Are you sure you want to leave?[y/n]\n";
			cin>>*op; strlower(op);
			if(*op=="y"||*op=="yes")
				break;
			getline(cin,*op);
		}
		else if(*op=="hello"||*op=="hi"||*op=="help"){
			cout<<"Hello, how may I help you?\n\n";
			cout<<"Here\'s the simple instruction:\n\nAdd task:\n    Format: add [Task name] [Category] [Importance] [Completed] [Note(optional)]\n\nView task:\n    Format1: view all [category/importance/completed (optional)]\n    Format2: view [category/importance/completed/order] [attribute]\n\nDelete task:\n    Format1: delete all [task/category/importance/completed] [attribute]\n    Format2: delete [task/category/importance/completed] [attribute] [Order in the list]\n\nModify task:\n    Format1: modify all [task/category/importance/completed] [attribute] [new_task_name] [new_category] [new_importance] [new_complete_state]\n    Format2: modify [task/category/importance/completed] [attribute] [Order in the list] [new_task_name] [new_category] [new_importance] [new_complete_state]\n\nUndo and redo:\n    Format: [undo/redo]\n\nSetting default value:\n    Format: set_default [Default_category] [Default_importance] [Default_complete_state]\n\nExport list data:\n    Format: export\n\nShow the To-Do list statistics:\n    Format: show_statistics\n\nShow log:\n    Format: show_log\n\nExit:\n    Format: exit\n\nTo view the detailed instruction, go README.md file.\nIf there is any inconsistency with README.md file, please follow the README.md file.\n\n";
		} 
		else
			cout<<"Unknown command \""<<*op<<"\", please try again.\n";
		delete ctg;	delete name; delete ipt; delete note; delete complt; delete ss;
	}
	delete op;
	delete nullstr;
	delete Data::cnt;
	delete Data::df_ipt;
	delete Data::df_cmplt;
	delete Data::df_ctg;
	delete DB;
	delete i;
	delete super_tmp;
	
	return 0;
}
/* Sample tests
add OOP Programming n y Object-Oriented Programming
add OOPDS-HW1 Homework nO YeS easy peasy lemon squeezy
add CompetitiveProgramming Programming y y
add 1000 long_code - y
add HW1-Sample - - -
set_default sample - y
set_default sample y y
add case1 - - -
add case2 - - -
add case3 - n -
add case4 - n n
add case5 - - n
add case1 sample2 - -
add case1 sample3 n -
add edge_case not_exist y y
view all
view all category
view all importance
view all completed
view task case1
view category Programming
view importance y
view completed n
view order 3
view all
delete all task case1
delete all category sample
delete all importance y
delete all completed y
undo
view all
undo
view all
undo
view all
undo
view all
view task case1
delete task case1 3
view category sample
delete category sample 2
view importance n
delete importance n 1
view completed n
delete completed n 2
undo
undo
view all
redo
undo
undo
undo
view all
modify all task case1 - sample100 n -
modify all category sample - sample2 - -
modify all importance y - important - -
modify all completed y completed_task - n -
undo
view all
modify task HW1-Sample 1 - - - y
modify task OOPDS-HW1 1 - - - y
view category important
modify category important 2 - - - n
view importance y
modify importance y 3 - TA - -
view completed y
modify completed y 2 - - - n
show_log
export
show_statistics
viewall
delete all
delete all pointer
delete all completed not_sure
view category basic75points
help
exit

add task_name category - n
add task_name2 category - y
add report sample - y
add report2 sample - n
view category sample
view completed y
view all
modify task task_name2 1 new_task_name new_category y y
modify task report2 1 - - - y
delete task new_task_name 1

add Programming Learning yes no
add Running Exercise - no It's on Monday.
add Math Learning yes no
add Math Homework yes no
add OOPDS-HW1 Homework - yes
add Digital_Design-HW1 Homework - yes
view importance yes
view task Math
show_statistics
delete all category Learning
delete task Running 1
show_statistics
undo
view all
redo
view all

add add add y y
add add add y n
add add add n n
add add add n y
add hi hi - -
add hi hi - y
view completed y
view all completed
modify all completed n - - - y
delete all completed y
*/
