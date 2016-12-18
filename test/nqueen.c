int print_board(int *board)
{
    int i, j;

    for (i = 0; i < 8; i++) {
        for (j = 0; j < 8; j++) {
            if (board[i * 8 + j]) {
                printf("Q ");
            } else {
                printf(". ");
            }
        }
        printf("\n");
    }
    printf("\n\n");
}

int conflict(int *board, int row, int col)
{
    int i;
    for (i = 0; i < row; i++) {
        if (board[i * 8 + col])
            return 1;
        int j = row - i;
        if (0 < col - j + 1 && board[i * 8 + col - j])
            return 1;
        if (col + j < 8 && board[i * 8 + col + j])
            return 1;
    }
    return 0;
}

void solve(int *board, int row)
{
    int i;

    if (row == 8) {
        print_board(board);
        return;
    }
    for (i = 0; i < 8; i++) {
        if (!conflict(board, row, i)) {
            board[row * 8 + i] = 1;
            solve(board, row + 1);
            board[row * 8 + i] = 0;
        }
    }
}

int main(void)
{
    int board[64];
    int i;
    for (i = 0; i < 64; i++) {
        board[i] = 0;
    }
    solve(board, 0);
    return 0;
}
