#include <bits/stdc++.h>
#include <algorithm>
#define int long long
using namespace std;
int n,m,u,v;
vector<int> Graph[5000005];
int vis[5000005];
 
void dfs(int x,int y){
	vis[x]=y=max(x,y);
	int len=Graph[x].size();
	for(int i=0;i<len;i++)
		if(vis[Graph[x][i]]>y){
			dfs(Graph[x][i],y);
			//break;
		}
	return;
} 
 
signed main() {
ios_base::sync_with_stdio(0);
cin.tie(0);
cout.tie(0);
cin>>n>>m;
for(int i=0;i<5000005;i++)
	vis[i]=INT_MAX;
for(int i=0;i<m;i++){
	cin>>u>>v;
	Graph[u].push_back(v);
	Graph[v].push_back(u);
}
for(int i=0;i<n;i++)
	sort(Graph[i].begin(),Graph[i].end());
dfs(0,0);
int max_tmp=0;
//for(int i=0;i<n;i++)
//	cout<<vis[i]<<' ';
//cout<<'\n';
cout<<1;
for(int i=1;i<n;i++){
	max_tmp=max(vis[i],max_tmp);
	if(max_tmp>vis[i])
		vis[i]=max_tmp;
	if(max_tmp<=i)
		cout<<'Y';
	else
		cout<<'N';
}
cout<<'\n';
}
