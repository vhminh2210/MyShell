#include<iostream>

using namespace std;

int main()
{
    int n = 100;
    cout<<"Enter n: ";
    cin>>n;
    for(int i=1;i<=n;++i) cout<<"HelloWorld "<<i<<'\n';
    ios_base::sync_with_stdio(false);
    cin.tie(NULL);
    cout.tie(NULL);
    return 0;
}
