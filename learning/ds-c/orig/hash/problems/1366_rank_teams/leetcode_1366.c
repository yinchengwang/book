#include<stdio.h>
#include<stdlib.h>

typedef struct {
    char ch;
    int rate[26];
} Team;

int len = 0;

int cmp(const void *a, const void *b)
{
    Team *a1 = (Team *)a;
    Team *b1 = (Team *)b;

    for (int i = 0; i < len; i++) {
        if (a1->rate[i] != b1->rate[i]) {
            return b1->rate[i] - a1->rate[i];
        }
    }

    return 0;
}

char *rankTeams(char ** votes, int votesSize)
{
    Team team[26];

    for (int i = 0; i < 26; i++) {
        team[i].ch = '0';
        for (int j = 0; j < 26; j++) {
            (team[i]).rate[j] = 0;
        }
    }

    len = strlen(votes[0]);

    for (int i = 0; i < votesSize; i++) {
        for (int j = 0; j < len; j++) {
            char ch = votes[i][j];
            team[ch - 'A'].ch = ch;
            (team[ch - 'A'].rate)[j]++;
        }
    }

    qsort(team, 26, sizeof(Team), cmp);

    char *ans = (char *)malloc(sizeof(char) * 27);
    int index = 0;
    for (int i = 0; i < 26; i++) {
        if (team[i].ch != '0') {
            ans[index++] = team[i].ch;
        }
    }

    ans[index] = '\0';

    return ans;
}