#include <bits/stdc++.h>
#include <algorithm>
#include <fstream>
#define int long long
using namespace std;

int Objects_8_Directions[6561][10];         //0~6560
int Closer_Bullets_8_Directions[6561][10];  //6561~13121
int Farther_Bullets_8_Directions[6561][10]; //13122~19682
int Enemy_8_Directions[3][6561][10];        //19683~26243 26244~32804 32805~39365
//Denominator,9actions

int idx_from_range(int l,int r,int *arr){
	int tmp=1,res=0;
	for(int i=r;i>=l;i--){
		res+=arr[i]*tmp;
		tmp*=3;
	}
	return res;
}

signed main(){
	ifstream data_input;
	ofstream data_output;
	string str;
	data_input.open("./Test01c.txt"); //Test01c
	data_output.open("./Results/Probability table.txt");
	getline(data_input, str);
	int lines_len=0;
	
	while(getline(data_input, str)){
		stringstream ss(str);
		string tmp;
		int ptr=0,hp=0,action;
		int idx[33];
		while(getline(ss,tmp,'\,')){
			if(ptr==0)
				hp=stoll(tmp);
			else if(ptr==33)
				action=stoll(tmp);
			else
				idx[ptr]=stoll(tmp);
			ptr++;
		}
		//for(int i=1;i<33;i++)	cout<<idx[i]<<" \n"[i==32];
		int idx1=idx_from_range(1,8,idx);
		//cout<<idx1<<'\n';
		Objects_8_Directions[idx1][action+1]++;
		Objects_8_Directions[idx1][0]++;
		idx1=idx_from_range(9,16,idx);
		Closer_Bullets_8_Directions[idx1][action+1]++;
		Closer_Bullets_8_Directions[idx1][0]++;
		idx1=idx_from_range(17,24,idx);
		Farther_Bullets_8_Directions[idx1][action+1]++;
		Farther_Bullets_8_Directions[idx1][0]++;
		idx1=idx_from_range(25,32,idx);
		if(hp<=100){
			Enemy_8_Directions[0][idx1][action+1]++;
			Enemy_8_Directions[0][idx1][0]++;
		}
		else if(hp<=200){
			Enemy_8_Directions[1][idx1][action+1]++;
			Enemy_8_Directions[1][idx1][0]++;
		}
		else{
			Enemy_8_Directions[2][idx1][action+1]++;
			Enemy_8_Directions[2][idx1][0]++;
		}
		lines_len++;
	}
	for(int i=0;i<6561;i++)
		for(int j=0;j<10;j++)
			data_output<<Objects_8_Directions[i][j]<<"\,\n"[j==9];
	for(int i=0;i<6561;i++)
		for(int j=0;j<10;j++)
			data_output<<Closer_Bullets_8_Directions[i][j]<<"\,\n"[j==9];
	for(int i=0;i<6561;i++)
		for(int j=0;j<10;j++)
			data_output<<Farther_Bullets_8_Directions[i][j]<<"\,\n"[j==9];
	for(int t=0;t<3;t++){
		for(int i=0;i<6561;i++)
			for(int j=0;j<10;j++)
				data_output<<Enemy_8_Directions[t][i][j]<<"\,\n"[j==9];
	}
	cout<<"Total "<<lines_len<<" lines\n";
	cout<<"Process finished!\n";
	data_input.close();
	data_output.close();
	//cout<<str<<'\n';
}
