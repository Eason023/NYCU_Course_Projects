#include <bits/stdc++.h> //dfs
#include <algorithm>
using namespace std; 
int n,m,u,v;
vector<int> Graph[5000005];
int p[5000005];
queue<int> q;

void dfs(int x,int lg){
	lg=max(x,lg);
	if(p[x]!=0x3f3f3f3f)
		return;
	p[x]=lg;
	for(int i=0;i<Graph[x].size();i++)
		dfs(Graph[x][i],lg);
	return;
}

signed main() {
ios_base::sync_with_stdio(0);
cin.tie(0);
cout.tie(0);
memset(p, 0x3f, sizeof(p));
cin>>n>>m;
for(int i=0;i<m;i++){
	cin>>u>>v;
	Graph[u].push_back(v);
	Graph[v].push_back(u);
}
dfs(0,0);
for(int i=0;i<n;i++){
	if(p[i]!=0)
		q.push(i);
	while((!q.empty())&&p[q.front()]<=i)
		q.pop();
	if(q.empty())
		cout<<'Y';
	else
		cout<<'N';
}
cout<<'\n';
}
