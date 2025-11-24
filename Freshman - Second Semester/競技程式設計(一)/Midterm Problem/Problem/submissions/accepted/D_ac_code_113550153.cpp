#include <bits/stdc++.h> //dsu union by size
#include <algorithm>
using namespace std; 
int n,m,u,v;
vector<int> Graph[5000005];
queue<int> q;
int p[5000005],sz[5000005],root;

int f(int x){
	return x ^ p[x] ? p[x] = f(p[x]) : x;
}
void uni(int x,int y){
	x=f(x);y=f(y);
	if(sz[y]<sz[x])
		swap(x,y);
	if(x ^ y){
		sz[y]+=sz[x];
		p[x]=y;
	}
	root=f(0);
	return;
}

signed main(){
	ios_base::sync_with_stdio(0);
	cin.tie(0);
	cout.tie(0);
	cin>>n>>m;
	for(int i=0;i<5000005;i++){
		p[i]=i;
		sz[i]=1;
	}
	for(int i=0;i<m;i++){
		cin>>u>>v;
		if(u>v)
			Graph[u].push_back(v);
		else
			Graph[v].push_back(u);
	}
	for(int i=0;i<n;i++){
		for(int j=0;j<Graph[i].size();j++)
			uni(i,Graph[i][j]);
		q.push(i);
		while((!q.empty())&&f(q.front())==root)
			q.pop();
		if(q.empty())
			cout<<'Y';
		else
			cout<<'N';
	}
	cout<<'\n';
}
