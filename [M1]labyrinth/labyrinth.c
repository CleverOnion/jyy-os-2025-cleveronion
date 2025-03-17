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
    ERR_MULTIPLE_EMPTY_AREAS,
    ERR_MOVE_FAILED
} ErrorCode;

// 函数声明
void print_version(void);
ErrorCode parse_arguments(int argc, char *argv[], char **map_filename, char **player_str, char **move_direction);
ErrorCode load_map(const char *filename, Map *map);
ErrorCode validate_map(const Map *map);
void deep_search(int x, int y, int visited[MAX_ROWS][MAX_COLS], const Map *map);
bool is_empty(int x, int y, const Map *map);
bool is_player(int x, int y, const Map *map, int player);
void print_map(const Map *map);
void trim_newline(char *str);
ErrorCode move_player(Map *map, int player, const char *direction);

int main(int argc, char *argv[])
{
    char *map_filename = NULL;
    char *player_str = NULL;
    char *move_direction = NULL;

    ErrorCode err = parse_arguments(argc, argv, &map_filename, &player_str, &move_direction);
    if (err != ERR_NONE)
    {
        fprintf(stderr, "Usage: %s -m <map_file> -p <player_id> [--move direction]\n", argv[0]);
        return 1;
    }

    // 校验玩家参数是否为单个数字
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

    // 地图验证（包括空区域检查）
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

    // 如果指定了移动命令，则执行移动操作
    if (move_direction != NULL)
    {
        err = move_player(&map, player, move_direction);
        if (err != ERR_NONE)
        {
            fprintf(stderr, "Move failed.\n");
            return 1;
        }
    }

    print_map(&map);
    return 0;
}

void print_version(void)
{
    printf("Labyrinth Game version 1.0\n");
}

ErrorCode parse_arguments(int argc, char *argv[], char **map_filename, char **player_str, char **move_direction)
{
    int opt;
    int has_map = 0, has_player = 0;
    *move_direction = NULL;
    int option_index = 0;
    static struct option long_options[] = {
        {"version", no_argument, 0, 'v'},
        {"map", required_argument, 0, 'm'},
        {"player", required_argument, 0, 'p'},
        {"move", required_argument, 0, 0}, // 仅支持长选项
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
        case 0: // 处理没有短选项的长选项
            if (strcmp(long_options[option_index].name, "move") == 0)
            {
                *move_direction = optarg;
            }
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
            // 采用 1 索引存储，便于边界检查
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

// 在 validate_map 中进行空区域检查
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
    // 注意：这里认为空地为 '.' 或者玩家（'1'-'9'）所在位置，
    // 主要用于连通区域搜索；而在移动时，仅允许移动到 '.' 的位置
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

// 根据 direction 移动指定玩家
ErrorCode move_player(Map *map, int player, const char *direction)
{
    int dx = 0, dy = 0;
    if (strcmp(direction, "up") == 0)
    {
        dx = -1;
    }
    else if (strcmp(direction, "down") == 0)
    {
        dx = 1;
    }
    else if (strcmp(direction, "left") == 0)
    {
        dy = -1;
    }
    else if (strcmp(direction, "right") == 0)
    {
        dy = 1;
    }
    else
    {
        return ERR_MOVE_FAILED; // 无效的移动方向
    }

    char playerChar = player + '0';
    int current_x = -1, current_y = -1;
    // 搜索地图中是否存在该玩家
    for (int i = 1; i <= map->rows; i++)
    {
        for (int j = 1; j <= map->cols; j++)
        {
            if (map->cells[i][j] == playerChar)
            {
                current_x = i;
                current_y = j;
                break;
            }
        }
        if (current_x != -1)
        {
            break;
        }
    }

    // 如果地图中没有该玩家，则将玩家放置在第一个空地
    if (current_x == -1)
    {
        for (int i = 1; i <= map->rows; i++)
        {
            for (int j = 1; j <= map->cols; j++)
            {
                if (map->cells[i][j] == '.')
                {
                    map->cells[i][j] = playerChar;
                    return ERR_NONE;
                }
            }
        }
        return ERR_MOVE_FAILED; // 没有空地放置
    }

    int target_x = current_x + dx;
    int target_y = current_y + dy;
    // 检查目标位置是否在地图范围内
    if (target_x < 1 || target_x > map->rows || target_y < 1 || target_y > map->cols)
    {
        return ERR_MOVE_FAILED;
    }
    // 目标位置必须为空白（即 '.'）
    if (map->cells[target_x][target_y] != '.')
    {
        return ERR_MOVE_FAILED;
    }

    // 执行移动：原位置置为 '.'，目标位置放置玩家
    map->cells[current_x][current_y] = '.';
    map->cells[target_x][target_y] = playerChar;
    return ERR_NONE;
}
