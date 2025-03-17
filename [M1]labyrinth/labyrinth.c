#include <stdio.h>
#include <stdlib.h>
#include <getopt.h>
#include <string.h>
#include <stdbool.h>

#define MAX_ROWS 110
#define MAX_COLS 110
#define MAX_MAP_DIM 100

typedef struct
{
    int rows;
    int cols;
    char cells[MAX_ROWS][MAX_COLS];
} Map;

typedef enum
{
    ERR_NONE,
    ERR_INVALID_ARGS,
    ERR_MAP_NOT_FOUND,
    ERR_INVALID_MAP,
    ERR_MULTIPLE_EMPTY_AREAS
} ErrorCode;

// 函数声明
void print_version(void);
ErrorCode parse_arguments(int argc, char *argv[], char **map_filename, char **player_str);
ErrorCode load_map(const char *filename, Map *map);
ErrorCode validate_map(const Map *map);
void deep_search(int x, int y, int visited[MAX_ROWS][MAX_COLS], const Map *map);
bool is_empty(int x, int y, const Map *map);
bool is_player(int x, int y, const Map *map, int player);
void print_map(const Map *map);
void trim_newline(char *str);

int main(int argc, char *argv[])
{
    char *map_filename = NULL;
    char *player_str = NULL;
    ErrorCode err = parse_arguments(argc, argv, &map_filename, &player_str);
    if (err != ERR_NONE)
    {
        fprintf(stderr, "Usage: %s -m <map_file> -p <player_number>\n", argv[0]);
        return 1;
    }

    if (player_str[0] < '0' || player_str[0] > '9' || player_str[1] != '\0')
    {
        fprintf(stderr, "Player must be a single digit between 0 and 9.\n");
        return 1;
    }
    int player = player_str[0] - '0';

    Map map;
    err = load_map(map_filename, &map);
    if (err != ERR_NONE)
    {
        fprintf(stderr, "Error loading map file: %d\n", err);
        return 1;
    }

    // 调用 validate_map 进行地图合法性检查（包括空区域检查）
    err = validate_map(&map);
    if (err == ERR_MULTIPLE_EMPTY_AREAS)
    {
        fprintf(stderr, "Map contains more than one empty area.\n");
        return 1;
    }
    else if (err != ERR_NONE)
    {
        fprintf(stderr, "Map validation failed: %d\n", err);
        return 1;
    }

    print_map(&map);
    return 0;
}

void print_version(void)
{
    printf("Labyrinth Game version 1.0\n");
}

ErrorCode parse_arguments(int argc, char *argv[], char **map_filename, char **player_str)
{
    int opt;
    int has_map = 0, has_player = 0;
    int option_index = 0;
    static struct option long_options[] = {
        {"version", no_argument, 0, 'v'},
        {"map", required_argument, 0, 'm'},
        {"player", required_argument, 0, 'p'},
        {0, 0, 0, 0}};

    while ((opt = getopt_long(argc, argv, "vm:p:", long_options, &option_index)) != -1)
    {
        switch (opt)
        {
        case 'v':
            print_version();
            exit(0);
        case 'm':
            has_map = 1;
            *map_filename = optarg;
            break;
        case 'p':
            has_player = 1;
            *player_str = optarg;
            break;
        case '?':
        default:
            return ERR_INVALID_ARGS;
        }
    }
    if (!has_map || !has_player)
    {
        return ERR_INVALID_ARGS;
    }
    return ERR_NONE;
}

ErrorCode load_map(const char *filename, Map *map)
{
    FILE *fp = fopen(filename, "r");
    if (!fp)
    {
        return ERR_MAP_NOT_FOUND;
    }
    char buffer[1024];
    map->rows = 0;
    while (fgets(buffer, sizeof(buffer), fp) != NULL)
    {
        trim_newline(buffer);
        int len = strlen(buffer);
        if (len == 0)
            continue; // 跳过空行
        if (map->rows == 0)
        {
            map->cols = len;
            if (map->cols < 1 || map->cols > MAX_MAP_DIM)
            {
                fclose(fp);
                return ERR_INVALID_MAP;
            }
        }
        else
        {
            if (len != map->cols)
            {
                fclose(fp);
                return ERR_INVALID_MAP;
            }
        }
        for (int i = 0; i < len; i++)
        {
            char c = buffer[i];
            if (c != '#' && c != '.' && !(c >= '0' && c <= '9'))
            {
                fclose(fp);
                return ERR_INVALID_MAP;
            }
            // 使用 1 索引存储，便于边界检查
            map->cells[map->rows + 1][i + 1] = c;
        }
        map->rows++;
        if (map->rows > MAX_MAP_DIM)
        {
            fclose(fp);
            return ERR_INVALID_MAP;
        }
    }
    fclose(fp);
    return ERR_NONE;
}

// 更新后的 validate_map 包含检查空区域个数是否为 1 的逻辑
ErrorCode validate_map(const Map *map)
{
    int visited[MAX_ROWS][MAX_COLS] = {0};
    int empty_area_count = 0;
    for (int i = 1; i <= map->rows; i++)
    {
        for (int j = 1; j <= map->cols; j++)
        {
            if (is_empty(i, j, map) && !visited[i][j])
            {
                deep_search(i, j, visited, map);
                empty_area_count++;
                if (empty_area_count > 1)
                {
                    return ERR_MULTIPLE_EMPTY_AREAS;
                }
            }
        }
    }
    return ERR_NONE;
}

void deep_search(int x, int y, int visited[MAX_ROWS][MAX_COLS], const Map *map)
{
    int dx[] = {-1, 1, 0, 0};
    int dy[] = {0, 0, -1, 1};
    visited[x][y] = 1;
    for (int i = 0; i < 4; i++)
    {
        int nx = x + dx[i], ny = y + dy[i];
        if (nx < 1 || nx > map->rows || ny < 1 || ny > map->cols)
        {
            continue;
        }
        if (!visited[nx][ny] && is_empty(nx, ny, map))
        {
            deep_search(nx, ny, visited, map);
        }
    }
}

bool is_empty(int x, int y, const Map *map)
{
    return (map->cells[x][y] == '.' || is_player(x, y, map, -1));
}

bool is_player(int x, int y, const Map *map, int player)
{
    if (player == -1)
    {
        return map->cells[x][y] >= '1' && map->cells[x][y] <= '9';
    }
    return map->cells[x][y] == (player + '0');
}

void print_map(const Map *map)
{
    for (int i = 1; i <= map->rows; i++)
    {
        for (int j = 1; j <= map->cols; j++)
        {
            printf("%c", map->cells[i][j]);
        }
        printf("\n");
    }
}

void trim_newline(char *str)
{
    size_t len = strlen(str);
    while (len > 0 && (str[len - 1] == '\n' || str[len - 1] == '\r'))
    {
        str[len - 1] = '\0';
        len--;
    }
}
