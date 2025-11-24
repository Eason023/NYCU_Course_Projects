#include <bits/stdc++.h>
#include <algorithm>
#define int long long
using namespace std;

int dp[605][605],h[605],weight,n,ans;

signed main(){
	ios::sync_with_stdio(0);
	cin.tie(0); cout.tie(0);
	cin>>n>>weight;
	for(int i=1;i<=n;i++)
		for(int j=2;j<=n;j++)
			dp[i][j]=LLONG_MAX;
	for(int i=1;i<=n;i++)
		cin>>h[i];
	for(int i=1;i<=n;i++){
		cin>>dp[i][1];
		if(dp[i][1]<=weight)
			ans=1;
	}
	for(int j=2;j<=n;j++){
		for(int i=1;i<=n;i++){
			int tmp=LLONG_MAX;
			for(int k=1;k<i;k++)
				for(int l=k;l<i;l++)
					if(h[l]<h[i])
						tmp=min(tmp,dp[l][j-1]);
			dp[i][j]=(tmp==LLONG_MAX?LLONG_MAX:tmp+dp[i][1]);
			if(dp[i][j]<=weight)
				ans=max(ans,j);
		}
	}
	cout<<ans<<'\n';
}
