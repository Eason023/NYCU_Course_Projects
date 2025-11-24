#include <bits/stdc++.h>
#include <algorithm>
#define int long long
using namespace std; 
int n,m,u,v;
vector<int> Graph[5000005];
queue<int> q;
int p[5000005];
int f(int x){
	return x ^ p[x] ? p[x] = f(p[x]) : x;
}
void uni(int x,int y){
	x=f(x);y=f(y);
	if(x<y)
		swap(x,y);
	if(x ^ y)
		p[x]=y;
	return;
}

signed main() {
ios_base::sync_with_stdio(0);
cin.tie(0);
cout.tie(0);
cin>>n>>m;
for(int i=0;i<5000005;i++)
	p[i]=i;
for(int i=0;i<m;i++){
	cin>>u>>v;
	Graph[u].push_back(v);
	Graph[v].push_back(u);
}
for(int i=0;i<n;i++){
	for(int j=0;j<Graph[i].size();j++)
		if(Graph[i][j]<i)
			uni(i,Graph[i][j]);
	if(p[i]!=0)
		q.push(i);
	while((!q.empty())&&f(q.front())==0)
		q.pop();
	if(q.empty())
		cout<<'Y';
	else
		cout<<'N';
}
cout<<'\n';
}
