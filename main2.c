#include <stdio.h>
#include <stdlib.h>
#include <unistd.h> // usleep 함수 사용
#include <termios.h> // 키 입력 감지용
#include <fcntl.h> // 파일 제어 옵션
#include <time.h> // rand 함수 초기화용
#include <string.h>

#define HEIGHT 20
#define WIDTH 80
#define MaxObstacles 5
#define Difficulty 15
#define MaxJumpHeight 5

#define SaveFile "score_save.txt"

char screen[HEIGHT+2][WIDTH+2];
int contHeight = 0; // 마지막 지형의 기울기를 알아내기 위한 변수
int score = 0; //점수
int contObs = 0; //생성되고 있는 장애물 모델 배열의 x 좌표
int obsCnt = 0; // 지금까지 생성되어 있는 장애물의 갯수
int obsColltime = 0; //장애물 생성 쿨타임
int isJumping = 0;
int jumpHeight = 0;
int currentDifficulty = 0;

typedef struct {
    int x, y;
    char model;
} Player;

typedef struct {
    char model[3][4]; // 2X3 크기의 장애물 모델
} Obstacle;

typedef struct {
    char name[20];
    int highscore;
} ScoreLog;

Player plr = {
    (WIDTH+2) / 2, // 캐릭터가 화면 정중앙에 위치하도록 초기화
    1, 
    'P' // 캐릭터 모델
};

Obstacle obsList[] = {
    {"/\\", "/\\", "|| "},
    {"/_\\", "|#|", "|#|"},
    {"   ", "/\\\\", "|||"},
    {"   ", "(*)", "|#|"}
    };

Obstacle currentObs;

ScoreLog topScore[5];

// 화면 초기화
void clearScreen() {
    system("clear"); //window는 cls
}

// 화면 출력 부분임
void printScreen() {
    char *ary;
    for(ary = &screen[0][0]; ary<=&screen[HEIGHT+1][WIDTH+1]; ary++){
        if(*ary == '\0'){
            printf(" ");
        }else{
            printf("%c", *ary);
        }
        if((ary - &screen[0][0] + 1) % (WIDTH+2) == 0){ // 개행 용도임 // 지금의 위치에서 배열 시작 부분의 위치값을 뺴고 너비로 나눠서 계산함
            printf("\n");
        }
    }
}

// 플레이어 위치에서 장애물 확인
int checkCollision() {
    if (screen[plr.y][plr.x] != '\0' && screen[plr.y][plr.x] != '_' && screen[plr.y][plr.x] != 'P') {
        screen[plr.y][plr.x] = 'P';
        return 1; // 충돌
    }
    return 0; // 안전
}

//캐릭터 그리기
void drawPlayer(){
    screen[plr.y][plr.x] = plr.model;
}

// 캐릭터 물리엔진
void physicPlayer() {
    if (isJumping) {
        if (jumpHeight < MaxJumpHeight) {
            plr.y--; // 위로 이동
            jumpHeight++;
        } else {
            isJumping = 0; // 점프 끝
        }
    } else {
        if (screen[plr.y][plr.x] == '_') { // 지형 위로 이동
            plr.y--; 
            jumpHeight = 0;
        }
        if (screen[plr.y + 1][plr.x] == '_') {// 지형 위로 돌아감
            jumpHeight = 0;
        } else if (plr.y < HEIGHT && screen[plr.y + 1][plr.x] == '\0' || plr.y < HEIGHT && screen[plr.y + 1][plr.x] == ' ' || plr.y < HEIGHT && screen[plr.y + 1][plr.x] != '_') { //중력으로 인해 밑으로 내려감
            plr.y++;
        }
    }
}

// 지형 초기화임
void initTerrains(int difficulty) {
    int x;
    int terrainsHeight = 0;
    int terrainsWidth = 0;
    char *ary;

    if(((WIDTH / difficulty) / 2) > (HEIGHT / 2)){
        terrainsWidth = difficulty * ((((WIDTH / difficulty) / 2)) - (HEIGHT / 2));
    }else{
        terrainsHeight = (HEIGHT / 2) - (((WIDTH / difficulty) / 2) - 1);
    }

    int slope = 1; // 내리막 경사 설정

    ary = &screen[terrainsHeight][0];

    for(x=1+terrainsWidth; x<WIDTH+1; x++){
        *(ary + x) = '_';

        if(x % difficulty == 0){
            ary += (WIDTH + 2)*slope;
            if (ary >= &screen[HEIGHT][0]){
                break;
            }
        }
    }
}

// 새로운 지형 생성
int spawnTerrain(int x, int y){
    char *ary = &screen[y][x];

    *ary = '_';

    return 2; // 장애물 쿨타임에 영향을 미침
}

// 장애물 생성임
int spawnObstacle(int x, int y) {
    int i;
    char * ary = &screen[y-2][x];

    for(i=0;i<3;i++){
        *(ary + i * (WIDTH+2)) = currentObs.model[i][contObs];
    }

    return 1;
}

// 윈쪽으로 장애물 이동
int shiftTerrain(int x, int heightGap, int lastHeight){
    int y;
    char *current, *next;
    int hasTerrain = 0; //지형 검사용

    for(y=1;y<HEIGHT;y++){ // 지형 복붙 for문
            current = &screen[y][x]; //현재 탐색할 x좌표
            next = &screen[y][x+1]; //다음에 탐색할 x좌표

            if(*current != '\0'){ //현재 지형을 공백으로 만들기
                *current = '\0';
            }

            if(*next != '\0' && *next != 'P' && x + 1 < WIDTH+1 && (current - (heightGap * (WIDTH + 2))) > &screen[0][WIDTH+1]){ // y좌표를 보정해서 윈쪽으로 옮기기(덮어씌우기)
                lastHeight = y;
                *(current - (heightGap * (WIDTH + 2))) = *next;
                hasTerrain = 1; // 지형 복사에 성공했음
            }

            current += (WIDTH + 2);
            next += (WIDTH + 2);
    }

    if (!hasTerrain) { // 지형 복사가 되지 않은 열일떄(즉 지형의 끝 부분임)
        // 경계 값 제한
        if (lastHeight < 1){
            lastHeight = 1;
        }
        if (lastHeight > HEIGHT - 1){
            lastHeight = HEIGHT - 1;
        }

        //if(1 == 2){ // 장애물 킬용 if문임 NVM!!
        if((rand()%100 <= 90 && contHeight % Difficulty <= Difficulty - 3 && obsColltime >= Difficulty*2) //랜덤 확률이면서 장애물이 생성될 공간이 있고 쿨타임이 없을때
        || contObs > 0){ //이미 생성되고 있는 장애물이 있는 경우

            if(contObs == 0){
                currentObs = obsList[rand() % 4];
                obsColltime = 0;
            }

            contObs += spawnObstacle(x, lastHeight); // 장애물의 모델 x 크기 증가 시키기
                
            if(contObs > 2){
                contObs = 0;
            }
        }else{
            obsColltime += spawnTerrain(x, lastHeight); //장애물이 소환되는 쿨타임 올리기
        }

        return 0;
    }
    return lastHeight;
}

// 지형 스크롤
void scrollTerrain(int difficulty) {
    int x;
    int lastHeight = 0;
    int heightGap = 0;

    if(contHeight % difficulty == 0){
        heightGap = 1;
        contHeight = 0;
    }

    for(x=1;x<=WIDTH;x++){ // 지형 윈쪽으로 스크롤
        int result = shiftTerrain(x, heightGap, lastHeight);

        if(result == 0){
            break;
        }else{
            lastHeight = result;
        }
    }
    ++contHeight;
}

// 비동기 입력 설정
void enableNonBlockingMode() {
    struct termios t;
    tcgetattr(STDIN_FILENO, &t);
    t.c_lflag &= ~ICANON; // Canonical 모드 비활성화
    t.c_lflag &= ~ECHO; // 입력된 키 출력 비활성화
    tcsetattr(STDIN_FILENO, TCSANOW, &t);

    int flags = fcntl(STDIN_FILENO, F_GETFL, 0);
    fcntl(STDIN_FILENO, F_SETFL, flags | O_NONBLOCK); // 논블로킹 설정
}

// 입력 복원
void disableNonBlockingMode() {
    struct termios t;
    tcgetattr(STDIN_FILENO, &t);
    t.c_lflag |= ICANON; // Canonical 모드 활성화
    t.c_lflag |= ECHO; // 입력된 키 출력 활성화
    tcsetattr(STDIN_FILENO, TCSANOW, &t);

    int flags = fcntl(STDIN_FILENO, F_GETFL, 0);
    fcntl(STDIN_FILENO, F_SETFL, flags & ~O_NONBLOCK); // 논블로킹 해제
}

// 게임 인트로
void initIntro(ScoreLog scores[], int totalScroes) {
    int i, j;

    const char *title = "{ Ski Jumping }";
    const char *prompt1 = "Press [ ENTER ] to start!!";
    const char *prompt2 = "Press [ Q ] to quit";

    int titleLen = strlen(title);
    int prompt1Len = strlen(prompt1);
    int prompt2Len = strlen(prompt2);

    // 화면 중앙에 텍스트 위치 계산용
    int titleRow = HEIGHT / 2 - 1;
    int prompt1Row = HEIGHT / 2 + 1;
    int prompt2Row = HEIGHT / 2 + 2;

    int titleCol = (WIDTH - titleLen) / 2;
    int prompt1Col = (WIDTH - prompt1Len) / 2;
    int prompt2Col = (WIDTH - prompt2Len) / 2;

    // 제목
    for (i = 0; i < titleLen; i++) {
        screen[titleRow][titleCol + i] = title[i];
    }

    // 프롬프트 "enter to start"
    for (i = 0; i < prompt1Len; i++) {
        screen[prompt1Row][prompt1Col + i] = prompt1[i];
    }

    // 프롬프트 "q to quit"
    for (i = 0; i < prompt2Len; i++){
        screen[prompt2Row][prompt2Col + i] = prompt2[i];
    }

    int scoreRow = HEIGHT - 6;
    // Top 5 점수 출력
    for(i = 0;i < 5 && i < totalScroes; i++){
        char scoreMsg[100];
        sprintf(scoreMsg, "%d. [ %s : %d ]", i+1, scores[i].name, scores[i].highscore);

        int scoreLen = strlen(scoreMsg);
        int scoreCol = (WIDTH - scoreLen) / 2;

        for (int j = 0; j < scoreLen; j++) {
            screen[scoreRow + i][scoreCol + j] = scoreMsg[j];
        }
    }
}

// 게임 점수 화면
void initOuttro(){
    int i;

    const char *title = "[Game Over]";

    char scoreMsg[20]; // 점수 텍스트
    sprintf(scoreMsg, "SCORE : %d", score);

    const char *prompt = "Enter Player name to save your SCORE!!";

    int titleLen = strlen(title);
    int scoreLen = strlen(scoreMsg);
    int promptLen = strlen(prompt);

    // 화면 중앙에 텍스트 위치 계산용
    int titleRow = HEIGHT / 3 - 1;
    int scoreRow = HEIGHT / 3 + 1;
    int promptRow = HEIGHT / 3 + 2;

    int titleCol = (WIDTH - titleLen) / 2;
    int scoreCol = (WIDTH - scoreLen) / 2;
    int promptCol = (WIDTH - promptLen) / 2;

    // 제목
    for (i = 0; i < titleLen; i++) {
        screen[titleRow][titleCol + i] = title[i];
    }

    // 점수 출력
    for(i = 0;i < scoreLen; i++){
        screen[scoreRow][scoreCol + i] = scoreMsg[i];
    }

    // 프롬프트 "enter to continue"
    for (i = 0; i < promptLen; i++) {
        screen[promptRow][promptCol + i] = prompt[i];
    }
}

void initValues(){
    contHeight = 0; // 마지막 지형의 기울기를 알아내기 위한 변수

    score = 0; //점수

    contObs = 0; //생성되고 있는 장애물 모델 배열의 x 좌표
    obsCnt = 0; // 지금까지 생성되어 있는 장애물의 갯수
    obsColltime = 0; //장애물 생성 쿨타임

    isJumping = 0;
    jumpHeight = 0;

    plr.y = 1; // 플레이어 위치 초기화
    plr.x = (WIDTH+2) / 2; 

    currentDifficulty = 0; // 난이도 초기화
}

// 파일 생성 함수
void createSaveFileIfNotExists() {
    FILE *file = fopen(SaveFile, "r");
    if (file == NULL) {
        // 파일이 없으면 생성
        file = fopen(SaveFile, "w");
        if (file == NULL) {
            perror("파일 생성 오류");
            exit(EXIT_FAILURE);
        }
        printf("새로운 파일 '%s'을 생성했습니다.\n", SaveFile);
    } else {
        fclose(file); // 파일이 존재하면 닫기
    }
}

// 점수 불러오기
void readScores(ScoreLog scores[], int *total){
    *total = 0;

    FILE *file = fopen(SaveFile, "r");
    if(file == NULL){
        return;
    }

    while (fscanf(file, "%19s %d", scores[*total].name, &scores[*total].highscore) == 2) {
        (*total)++;
    }
    
    fclose(file);
}

// 점수 저장
void saveScore(char *name, int score){
    FILE *file = fopen(SaveFile, "a");
    if(file == NULL){
        return;
    }

    fprintf(file, "%s %d\n", name, score);
    fclose(file);
}

// 불러온 점수 정렬(내림차순)
void sortScores(ScoreLog scores[], int total){
    int i, j;

    for (i=0;i<total-1;i++) {
        for (j=i+1;j<total;j++) {
            if (scores[i].highscore < scores[j].highscore) {
                ScoreLog temp = scores[i];
                scores[i] = scores[j];
                scores[j] = temp;
            }
        }
    }
}

// 화면 초기화(1은 완전 초기화)
void initScreen(int mode) {
    int i;
    char *ary;

    if(mode == 1){
        for(ary = &screen[0][0]; ary <= &screen[HEIGHT+1][WIDTH+1]; ary++){
            *ary = '\0';
        }
    }

    ary = &screen[0][0];

    for(i=0;i <= WIDTH+1; i++){
        *(ary + i) = '='; // 위쪽 테두리
        *(ary + ((WIDTH+2)*(HEIGHT+1)) + i) = '='; // 아랫쪽 테두리 // 0에다 (높이 * 너비)값을 더해 배열의 마지박 줄 위치를 계산
    }

    for (i=0; i <= HEIGHT+1; i++) {
        *(ary + i * (WIDTH + 2)) = '|'; // 좌측 테두리
        *(ary + i * (WIDTH + 2) + WIDTH + 1) = '|'; // 우측 테두리
    }
}

// 메인 함수
int main(void) {
    enableNonBlockingMode(); // 비동기 입력 모드 활성화

    createSaveFileIfNotExists();

    while (1)
    {   
        enableNonBlockingMode(); // 비동기 입력 모드 활성화

        ScoreLog scores[100];
        int totalScores = 0;

        readScores(scores, &totalScores);
        if(totalScores > 0){
            sortScores(scores, totalScores);
        }

        clearScreen();
        initScreen(1);
        initIntro(scores, totalScores); // 인트로 화면 저장
        printScreen(); // 인트로 화면 출력

        while (1) {
            char ch = getchar();
            if (ch == '\n') { // 엔터 키로 시작
                break;
            }else if(ch == 'q' || ch == 'Q'){
                return 0;
            }
        }

        clearScreen();

        srand(time(NULL));
        initScreen(1); // 화면 초기화
        initTerrains(Difficulty); // 초기 지형 생성

        while (1) { // 반복 실행
            char ch = getchar(); // 키 입력 감지
            if (ch == 'q') { // 'q' 입력 시 종료 기능
                break;
            }
            if ((ch == ' ') && !isJumping) { // 점프 키 입력 기능임
                isJumping = 1;
            }

            currentDifficulty = Difficulty - (score / 100); //점수에 따라 난이도 조정

            if(currentDifficulty < 5){
                currentDifficulty = 5;
            }

            scrollTerrain(currentDifficulty);
            int speed = 150000 - (score * 10); // 기본 속도 0.15초
            if(speed < 100000){
                speed = 100000;
            } 
            usleep(speed); // (속도 조절)
            physicPlayer();
            if (checkCollision() == 1) {
                break;
            }
            drawPlayer(); // 플레이어 위치 유지
            clearScreen();
            score += 1;
            printScreen(); // 화면 출력
        } 

        clearScreen();
        initOuttro();
        printScreen();

        disableNonBlockingMode(); // 비동기 입력 모드 비활성화

        while(1){
            char playerName[20];
            fgets(playerName, sizeof(playerName), stdin);

            int len = strlen(playerName);
            if(len > 0 && playerName[len - 1] == '\n'){
                playerName[len - 1] = '\0';
            }

            len = strlen(playerName);
            if(len > 0){
                saveScore(playerName, score); //플레이어의 점수 저장
                break;
            }else{
                break;
            }
        }

        initValues();
    }

    disableNonBlockingMode(); // 비동기 입력 모드 비활성화

    printf("게임을 종료합니다.\n");
    
    return 0;
}
