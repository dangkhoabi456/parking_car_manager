#include <gtk/gtk.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>
#include <ctype.h>
#include <stdbool.h>
#define HOURLY_RATE 5000  // đơn giá gửi xe mỗi giờ

typedef struct {
    char license_plate[20];   // biển số xe
    int fee;                  // số tiền
    time_t entry_time;        // thời gian vào
    clock_t clock_start;// thời gian bắt đầu tính giờ riêng
    int floor; 
} vehicle;
typedef struct {
    GtkListStore *store_tang1;
    GtkListStore *store_tang2;
    GtkWindow *parent_window;
} SharedData;

#define MAX_SLOTS 100
vehicle vehicle_list[MAX_SLOTS];
void load_history_data(GtkListStore *store);
int num_vehicles = 0;
double doanh_thu = 0;  // biến toàn cục
GtkWidget *label_stats; // Dùng để cập nhật hiển thị thống kê
void Cal_total(double fee);  // Khai báo hàm Cal_total
void load_treeviews(SharedData *shared_data);
int has_available_slot();
static void ThanhtoanvaXoa(GtkWidget *widget, gpointer data);
static void onNhapBienSoXe(GtkWidget *widget, gpointer data); // prototype
int Check__license_plate(const char *a);  // Khai báo prototype
void read_form_file(SharedData *shared_data);
vehicle* find_vehicle(const char *license_plate);
void update_statistics_display();
static void on_search_changed(GtkEditable *entry, gpointer user_data);
void save_doanh_thu();
void load_doanh_thu();
void log_action(const char *license_plate, const char *action, int fee);
// Hàm lưu doanh thu
void save_doanh_thu() {
    FILE *f = fopen("doanh_thu.txt", "w");
    if (f) {
        fprintf(f, "%.0f", doanh_thu);
        fclose(f);
    }
}

// Hàm đọc doanh thu
void load_doanh_thu() {
    FILE *f = fopen("doanh_thu.txt", "r");
    if (f) {
        fscanf(f, "%lf", &doanh_thu);
        fclose(f);
    }
}

// Thay đổi hàm read_from_file() để tương thích Windows
void read_from_file() {
    FILE *pt = fopen("parking_data.txt", "r");
    if (pt == NULL) {
        pt = fopen("parking_data.txt", "w"); // Tạo file nếu chưa có
        if (pt) fclose(pt);
        return;
    }

    vehicle temp;
    int year, mon, day, hour, min, sec;
    num_vehicles = 0;

    while (fscanf(pt, "%s %d %d-%d-%d %d:%d:%d %d",
                  temp.license_plate, &temp.fee,
                  &year, &mon, &day, &hour, &min, &sec, &temp.floor) == 9) {
        struct tm tm_time = {0};
        tm_time.tm_year = year - 1900;
        tm_time.tm_mon = mon - 1;
        tm_time.tm_mday = day;
        tm_time.tm_hour = hour;
        tm_time.tm_min = min;
        tm_time.tm_sec = sec;

        temp.entry_time = mktime(&tm_time);
        temp.clock_start = clock() - (clock_t)(difftime(time(NULL), temp.entry_time) * CLOCKS_PER_SEC);

        if (num_vehicles < MAX_SLOTS) {
            vehicle_list[num_vehicles++] = temp;
        }
    }
    fclose(pt);
}


void log_action(const char *license_plate, const char *action, int fee) {
    FILE *log = fopen("log.txt", "a");
    if (!log) return;

    time_t now = time(NULL);
    char time_str[30];
    strftime(time_str, sizeof(time_str), "%Y-%m-%d %H:%M:%S", localtime(&now));

    if (strcmp(action, "out") == 0) {
        fprintf(log, "%s %s %d %s\n", license_plate, action, fee, time_str);
    } else {
        fprintf(log, "%s %s %s\n", license_plate, action, time_str);
    }
    fclose(log);
}
// Hàm ghi toàn bộ dữ liệu hiện tại vào file (dùng khi thay đổi trạng thái)
// Hàm ghi toàn bộ dữ liệu bãi xe
void save_parking_data() {
    FILE *f = fopen("parking_data.txt", "w");
    if (!f) {
        printf("Lỗi mở file để ghi!\n");
        return;
    }

    char time_str[30];
    for (int i = 0; i < num_vehicles; i++) {
        strftime(time_str, sizeof(time_str), "%Y-%m-%d %H:%M:%S", 
                localtime(&vehicle_list[i].entry_time));
        fprintf(f, "%s %d %s %d\n",
               vehicle_list[i].license_plate,
               vehicle_list[i].fee,
               time_str,
               vehicle_list[i].floor);
    }
    fclose(f);
}


// Kiểm tra còn chỗ không
int has_available_slot() {
    return num_vehicles < MAX_SLOTS;
}

// Hàm callback được gọi khi ứng dụng khởi động
static void activate(GtkApplication *app, gpointer user_data) {
    GtkWidget *window;
    GtkWidget *button;
    GtkWidget *button_box;
    GtkWidget *label;
    GtkWidget *containerBox;
    // Tạo cửa sổ chính
    window = gtk_application_window_new(app);
    gtk_window_set_title(GTK_WINDOW(window), "Quản lý bãi giữ xe");
    gtk_window_set_default_size(GTK_WINDOW(window), 500, 500);

    // Tạo hộp chứa theo chiều dọc
    containerBox = gtk_box_new(GTK_ORIENTATION_VERTICAL, 5);

    // ==== TẠO VÀ THÊM 3 NÚT GÓC TRÊN PHẢI ====
    GtkWidget *top_right_box = gtk_box_new(GTK_ORIENTATION_HORIZONTAL, 5);

    GtkWidget *btn1 = gtk_button_new_with_label("Thêm");
    GtkWidget *btn2 = gtk_button_new_with_label("Xóa và Thanh toán");

    gtk_box_pack_start(GTK_BOX(top_right_box), btn1, FALSE, FALSE, 0);
    gtk_box_pack_start(GTK_BOX(top_right_box), btn2, FALSE, FALSE, 0);

    gtk_widget_set_halign(top_right_box, GTK_ALIGN_END);
    gtk_box_pack_start(GTK_BOX(containerBox), top_right_box, FALSE, FALSE, 5);

       // ==== TẠO NOTEBOOK (CÁC TAB) ====
    GtkWidget *notebook = gtk_notebook_new();

    // === TAB 1: Trang chủ ===
    GtkWidget *tab_label1 = gtk_label_new("Trang chủ");
    GtkWidget *home_box = gtk_box_new(GTK_ORIENTATION_VERTICAL, 5);
    GtkWidget *label_welcome = gtk_label_new("Chào mừng đến với hệ thống quản lý bãi giữ xe");
    GtkWidget *label_fee = gtk_label_new("Phí giữ xe: 5.000 VND");
    GtkWidget *label_note = gtk_label_new("Phí giữ xe được tính theo giờ.Nếu bạn gửi xe chưa đủ 1 giờ thì vẫn tính tròn là 1 giờ.");
    gtk_box_pack_start(GTK_BOX(home_box), label_welcome, FALSE, FALSE, 0);
    gtk_box_pack_start(GTK_BOX(home_box), label_fee, FALSE, FALSE, 0);
    gtk_box_pack_start(GTK_BOX(home_box), label_note, FALSE, FALSE, 0);
    // Gắn box vào tab Trang chủ
    gtk_notebook_append_page(GTK_NOTEBOOK(notebook), home_box, tab_label1);

    // === TAB 2: Thống kê ===
    GtkWidget *tab_label2 = gtk_label_new("Thống kê");
GtkWidget *tab_content2 = gtk_box_new(GTK_ORIENTATION_VERTICAL, 10);

label_stats = gtk_label_new("Đang tải thống kê...");
gtk_box_pack_start(GTK_BOX(tab_content2), label_stats, FALSE, FALSE, 10);

update_statistics_display();

    gtk_notebook_append_page(GTK_NOTEBOOK(notebook), tab_content2, tab_label2);

    // === TAB 3: Bãi xe ===
GtkWidget *tab_label3 = gtk_label_new("Bãi xe");

// Tạo notebook con để chứa các tầng
GtkWidget *nested_notebook = gtk_notebook_new();
GtkWidget *bai_xe_vbox = gtk_box_new(GTK_ORIENTATION_VERTICAL, 5);

// Thanh tìm kiếm
GtkWidget *search_entry = gtk_entry_new();
gtk_entry_set_placeholder_text(GTK_ENTRY(search_entry), "Tìm biển số xe...");
gtk_box_pack_start(GTK_BOX(bai_xe_vbox), search_entry, FALSE, FALSE, 0);


// === Tầng 1 ===
GtkWidget *tab_tang1 = gtk_label_new("Tầng 1");

// Model và TreeView cho tầng 1
GtkListStore *store_tang1 = gtk_list_store_new(1, G_TYPE_STRING);


GtkWidget *treeview_tang1 = gtk_tree_view_new_with_model(GTK_TREE_MODEL(store_tang1));
GtkCellRenderer *renderer1 = gtk_cell_renderer_text_new();
GtkTreeViewColumn *column1 = gtk_tree_view_column_new_with_attributes("Biển số xe", renderer1, "text", 0, NULL);
gtk_tree_view_append_column(GTK_TREE_VIEW(treeview_tang1), column1);

GtkWidget *scroll_tang1 = gtk_scrolled_window_new(NULL, NULL);
gtk_container_add(GTK_CONTAINER(scroll_tang1), treeview_tang1);
gtk_widget_set_vexpand(scroll_tang1, TRUE);
gtk_widget_set_hexpand(scroll_tang1, TRUE);

gtk_notebook_append_page(GTK_NOTEBOOK(nested_notebook), scroll_tang1, tab_tang1);

// === Tầng 2 ===
GtkWidget *tab_tang2 = gtk_label_new("Tầng 2");

// Model và TreeView cho tầng 2
GtkListStore *store_tang2 = gtk_list_store_new(1, G_TYPE_STRING);


GtkWidget *treeview_tang2 = gtk_tree_view_new_with_model(GTK_TREE_MODEL(store_tang2));
GtkCellRenderer *renderer2 = gtk_cell_renderer_text_new();
GtkTreeViewColumn *column2 = gtk_tree_view_column_new_with_attributes("Biển số xe", renderer2, "text", 0, NULL);
gtk_tree_view_append_column(GTK_TREE_VIEW(treeview_tang2), column2);

GtkWidget *scroll_tang2 = gtk_scrolled_window_new(NULL, NULL);
gtk_container_add(GTK_CONTAINER(scroll_tang2), treeview_tang2);
gtk_widget_set_vexpand(scroll_tang2, TRUE);
gtk_widget_set_hexpand(scroll_tang2, TRUE);

gtk_notebook_append_page(GTK_NOTEBOOK(nested_notebook), scroll_tang2, tab_tang2);

// Cuối cùng: Thêm notebook con vào tab chính "Bãi xe"
gtk_box_pack_start(GTK_BOX(bai_xe_vbox), nested_notebook, TRUE, TRUE, 0);
gtk_notebook_append_page(GTK_NOTEBOOK(notebook), bai_xe_vbox, tab_label3);

// Khởi tạo SharedData
SharedData *shared_data = g_new(SharedData, 1);
shared_data->store_tang1 = store_tang1;
shared_data->store_tang2 = store_tang2;
// Nếu cần truyền cửa sổ chính
shared_data->parent_window = GTK_WINDOW(window);  // Chỉ nếu bạn dùng trong hàm xử lý
g_signal_connect(search_entry, "changed", G_CALLBACK(on_search_changed), shared_data);
g_signal_connect(btn2, "clicked", G_CALLBACK(ThanhtoanvaXoa), shared_data);

 load_doanh_thu();
read_from_file();
load_treeviews(shared_data);

// Kết nối nút với callback và truyền shared_data
g_signal_connect(btn1, "clicked", G_CALLBACK(onNhapBienSoXe), shared_data);
    
// Thêm notebook vào container chính
gtk_box_pack_start(GTK_BOX(containerBox), notebook, TRUE, TRUE, 5);

// Tạo scrolled window chứa bảng lịch sử
GtkWidget *scrolled_history = gtk_scrolled_window_new(NULL, NULL);
gtk_scrolled_window_set_policy(GTK_SCROLLED_WINDOW(scrolled_history), GTK_POLICY_AUTOMATIC, GTK_POLICY_AUTOMATIC);

// Tạo TreeView
GtkWidget *history_view = gtk_tree_view_new();
gtk_container_add(GTK_CONTAINER(scrolled_history), history_view);

// Tạo model cho TreeView
GtkListStore *store = gtk_list_store_new(5,
    G_TYPE_INT,     // STT
    G_TYPE_STRING,  // Biển số xe
    G_TYPE_STRING,  // Trạng thái
    G_TYPE_STRING,  // Thời gian
    G_TYPE_STRING   // Chi phí giữ xe (tạm dạng chuỗi)
);
gtk_tree_view_set_model(GTK_TREE_VIEW(history_view), GTK_TREE_MODEL(store));
g_object_unref(store);
load_history_data(store);

// Thêm các cột
const char *titles[] = {"STT", "Biển số xe", "Trạng thái", "Thời gian lúc lấy xe", "Chi phí giữ xe"};
for (int i = 0; i < 5; i++) {
    GtkCellRenderer *renderer = gtk_cell_renderer_text_new();
    GtkTreeViewColumn *column = gtk_tree_view_column_new_with_attributes(titles[i], renderer, "text", i, NULL);
    gtk_tree_view_append_column(GTK_TREE_VIEW(history_view), column);
}

// Thêm tab vào notebook
gtk_notebook_append_page(GTK_NOTEBOOK(notebook), scrolled_history, gtk_label_new("Lịch sử giữ xe"));

// Thêm vào cửa sổ & hiển thị
gtk_container_add(GTK_CONTAINER(window), containerBox);
gtk_widget_show_all(window);
    // Đọc dữ liệu và load vào TreeView khi app mở lên

}
void load_history_data(GtkListStore *store) {
    FILE *log = fopen("log.txt", "r");
    if (!log) return;

    int stt = 1;
    char license[20], status[10];
    int fee;
    char time_str[50];

    while (!feof(log)) {
        int read = fscanf(log, "%s %s", license, status);
        if (read != 2) break;

        GtkTreeIter iter;

        if (strcmp(status, "out") == 0) {
            fscanf(log, "%d %[^\n]", &fee, time_str);

            char fee_str[20];
            snprintf(fee_str, sizeof(fee_str), "%d VND", fee);

            gtk_list_store_append(store, &iter);
            gtk_list_store_set(store, &iter,
                0, stt++,
                1, license,
                2, "Ra",
                3, time_str,
                4, fee_str,
                -1
            );
        } else {
            fscanf(log, "%[^\n]", time_str);
            gtk_list_store_append(store, &iter);
            gtk_list_store_set(store, &iter,
                0, stt++,
                1, license,
                2, "Vào",
                3, time_str,
                4, "-", // Không có phí lúc vào
                -1
            );
        }
    }

    fclose(log);
}

// Hàm kiểm tra cú pháp biển số
int Check__license_plate(const char *a) {
    int count = 0;
    if (strlen(a) == 10) {
        if (isdigit(a[0]) && isdigit(a[1])) count++;
        if (isalpha(a[2])) count++;
        if (a[3] == '-') count++;
        if (isdigit(a[4]) && isdigit(a[5]) && isdigit(a[6])) count++;
        if (a[7] == '.') count++;
        if (isdigit(a[8]) && isdigit(a[9])) count++;
    }
    return count == 6; // Trả về 1 nếu hợp lệ, 0 nếu không hợp lệ
}
//load dữ liệu từ file parking_data.txt
void load_treeviews(SharedData *shared_data) {
    gtk_list_store_clear(shared_data->store_tang1);
    gtk_list_store_clear(shared_data->store_tang2);
    GtkTreeIter iter;

    for (int i = 0; i < num_vehicles; i++) {
        if (vehicle_list[i].floor == 1) {
            gtk_list_store_append(shared_data->store_tang1, &iter);
            gtk_list_store_set(shared_data->store_tang1, &iter, 0, vehicle_list[i].license_plate, -1);
        } else if (vehicle_list[i].floor == 2) {
            gtk_list_store_append(shared_data->store_tang2, &iter);
            gtk_list_store_set(shared_data->store_tang2, &iter, 0, vehicle_list[i].license_plate, -1);
        }
    }
}

// Hàm nhập biển số và thêm xe
static void onNhapBienSoXe(GtkWidget *widget, gpointer data) {
  SharedData *info = (SharedData*)data;
    GtkListStore *store1 = info->store_tang1;
    GtkListStore *store2 = info->store_tang2;
    GtkWindow *parent_window = info->parent_window;
    GtkWidget *dialog, *content_area, *entry_plate, *entry_floor;
    dialog = gtk_dialog_new_with_buttons("Nhập thông tin xe", parent_window,
                                         GTK_DIALOG_MODAL,
                                         "_OK", GTK_RESPONSE_OK,
                                         "_Hủy", GTK_RESPONSE_CANCEL, NULL);

    content_area = gtk_dialog_get_content_area(GTK_DIALOG(dialog));

    GtkWidget *grid = gtk_grid_new();
    gtk_grid_set_row_spacing(GTK_GRID(grid), 5);
    gtk_grid_set_column_spacing(GTK_GRID(grid), 5);

    GtkWidget *label_plate = gtk_label_new("Biển số:");
    entry_plate = gtk_entry_new();
    gtk_entry_set_placeholder_text(GTK_ENTRY(entry_plate), "VD: 59A-123.45");

    GtkWidget *label_floor = gtk_label_new("Tầng (1 hoặc 2):");
    entry_floor = gtk_entry_new();
    gtk_entry_set_placeholder_text(GTK_ENTRY(entry_floor), "1");

    gtk_grid_attach(GTK_GRID(grid), label_plate, 0, 0, 1, 1);
    gtk_grid_attach(GTK_GRID(grid), entry_plate, 1, 0, 1, 1);
    gtk_grid_attach(GTK_GRID(grid), label_floor, 0, 1, 1, 1);
    gtk_grid_attach(GTK_GRID(grid), entry_floor, 1, 1, 1, 1);

    gtk_container_add(GTK_CONTAINER(content_area), grid);
    gtk_widget_show_all(dialog);

    if (gtk_dialog_run(GTK_DIALOG(dialog)) == GTK_RESPONSE_OK) {
        const gchar *plate_input = gtk_entry_get_text(GTK_ENTRY(entry_plate));
        const gchar *floor_input = gtk_entry_get_text(GTK_ENTRY(entry_floor));

        // Kiểm tra định dạng biển số
        if (!Check__license_plate(plate_input)) {
            GtkWidget *err = gtk_message_dialog_new(parent_window, GTK_DIALOG_MODAL, GTK_MESSAGE_ERROR,
                                                    GTK_BUTTONS_CLOSE, "Biển số không hợp lệ!\nĐịnh dạng: XXA-XXX.XX");
            gtk_dialog_run(GTK_DIALOG(err));
            gtk_widget_destroy(err);
            gtk_widget_destroy(dialog);
            return;
        }

        int floor = atoi(floor_input);
        if (floor != 1 && floor != 2) {
            GtkWidget *err = gtk_message_dialog_new(parent_window, GTK_DIALOG_MODAL, GTK_MESSAGE_ERROR,
                                                    GTK_BUTTONS_CLOSE, "Chỉ chấp nhận tầng 1 hoặc 2.");
            gtk_dialog_run(GTK_DIALOG(err));
            gtk_widget_destroy(err);
            gtk_widget_destroy(dialog);
            return;
        }

        // Kiểm tra trùng biển số
        for (int i = 0; i < num_vehicles; i++) {
            if (strcmp(plate_input, vehicle_list[i].license_plate) == 0) {
                GtkWidget *err = gtk_message_dialog_new(parent_window, GTK_DIALOG_MODAL, GTK_MESSAGE_ERROR,
                                                        GTK_BUTTONS_CLOSE, "Biển số này đã tồn tại!");
                gtk_dialog_run(GTK_DIALOG(err));
                gtk_widget_destroy(err);
                gtk_widget_destroy(dialog);
                return;
            }
        }

        // Nếu mọi thứ hợp lệ, thêm vào danh sách
        vehicle new_vehicle;
        strncpy(new_vehicle.license_plate, plate_input, sizeof(new_vehicle.license_plate));
        new_vehicle.floor = floor;
        new_vehicle.entry_time = time(NULL);
        new_vehicle.clock_start = clock();
        new_vehicle.fee = 0;
        vehicle_list[num_vehicles++] = new_vehicle;
        save_parking_data();
         GtkTreeIter iter;
        if (floor == 1) {
            gtk_list_store_append(store1, &iter);
            gtk_list_store_set(store1, &iter, 0, plate_input, -1);
        } else {
            gtk_list_store_append(store2, &iter);
            gtk_list_store_set(store2, &iter, 0, plate_input, -1);
        }

        g_print("Xe %s da duoc them thanh cong o tang %d!\n", plate_input, floor);
        log_action(plate_input, "in",0);
		update_statistics_display();

    }

    gtk_widget_destroy(dialog);
}

vehicle* find_vehicle(const char *license_plate) {
    for (int i = 0; i < num_vehicles; i++) {
        if (strcmp(vehicle_list[i].license_plate, license_plate) == 0) {
            return &vehicle_list[i];
        }
    }
    return NULL;
}
void Cal_total(double fee){
	doanh_thu += fee;
	save_doanh_thu();
}//gọi hàm Cal_total(fee) ở trong hàm void remove_vehicle(const char *license_plate)
// Hàm xử lý sự kiện khi nhấn nút
static void ThanhtoanvaXoa(GtkWidget *widget, gpointer data) {
    SharedData *info = (SharedData*)data;
    GtkWindow *parent_window = info->parent_window;

    GtkWidget *dialog, *entry;
    dialog = gtk_dialog_new_with_buttons("Thanh toán & Xóa xe",
                                         parent_window,
                                         GTK_DIALOG_MODAL,
                                         "_OK", GTK_RESPONSE_OK,
                                         "_Hủy", GTK_RESPONSE_CANCEL, NULL);

    GtkWidget *content_area = gtk_dialog_get_content_area(GTK_DIALOG(dialog));
    GtkWidget *label = gtk_label_new("Nhập biển số xe:");
    entry = gtk_entry_new();
    gtk_entry_set_placeholder_text(GTK_ENTRY(entry), "VD: 59A-123.45");

    GtkWidget *box = gtk_box_new(GTK_ORIENTATION_VERTICAL, 5);
    gtk_box_pack_start(GTK_BOX(box), label, FALSE, FALSE, 0);
    gtk_box_pack_start(GTK_BOX(box), entry, FALSE, FALSE, 0);
    gtk_container_add(GTK_CONTAINER(content_area), box);
    gtk_widget_show_all(dialog);

    if (gtk_dialog_run(GTK_DIALOG(dialog)) == GTK_RESPONSE_OK) {
        const gchar *plate_input = gtk_entry_get_text(GTK_ENTRY(entry));
        vehicle *veh = find_vehicle(plate_input);
        if (veh == NULL) {
            GtkWidget *err = gtk_message_dialog_new(parent_window, GTK_DIALOG_MODAL, GTK_MESSAGE_ERROR,
                                                    GTK_BUTTONS_CLOSE, "Không tìm thấy xe!");
            gtk_dialog_run(GTK_DIALOG(err));
            gtk_widget_destroy(err);
        } else {
            // Tính thời gian gửi
            clock_t clock_end = clock();
            double elapsed_seconds = (double)(clock_end - veh->clock_start) / CLOCKS_PER_SEC;
            int hours = (int)(elapsed_seconds / 3600);
   			int minutes = ((int)(elapsed_seconds) % 3600) / 60;
    		int seconds = (int)elapsed_seconds % 60;
            int total_hours = (elapsed_seconds + 3599) / 3600;
            veh->fee = total_hours * HOURLY_RATE;
            Cal_total(veh->fee); // Cộng doanh thu
            log_action(plate_input, "out",veh->fee);
			update_statistics_display();


            // Hiển thị phí
            char msg[100];
            snprintf(msg, sizeof(msg), "Xe %s\nThời gian gửi: %.1f giờ\nPhí: %d VND", plate_input, elapsed_seconds / 3600, veh->fee);
            GtkWidget *info = gtk_message_dialog_new(parent_window, GTK_DIALOG_MODAL, GTK_MESSAGE_INFO,
                                                     GTK_BUTTONS_OK, "%s", msg);
            gtk_dialog_run(GTK_DIALOG(info));
            gtk_widget_destroy(info);

            // Xóa khỏi danh sách
            for (int i = 0; i < num_vehicles; i++) {
                if (strcmp(vehicle_list[i].license_plate, plate_input) == 0) {
                    for (int j = i; j < num_vehicles - 1; j++)
                        vehicle_list[j] = vehicle_list[j + 1];
                    num_vehicles--;
                    break;
                }
            }
			save_parking_data();
            // Ghi lại file mới
      
            // Cập nhật giao diện
         load_treeviews((SharedData*)data);  

        }
    }

    gtk_widget_destroy(dialog);
}
void update_statistics_display() {
    int in_count = 0, out_count = 0;
    // Luôn đọc lại doanh thu từ file
    load_doanh_thu();

    FILE *log = fopen("log.txt", "r");
    if (log) {
        char line[256];
        while (fgets(line, sizeof(line), log)) {
            if (strstr(line, " in ") != NULL) in_count++;
            else if (strstr(line, " out ") != NULL) out_count++;
        }
        fclose(log);
    }

    char stats[256];
    snprintf(stats, sizeof(stats),
             "Xe vào: %d\nXe ra: %d\nDoanh thu: %.0f VND",
             in_count, out_count, doanh_thu);

    gtk_label_set_text(GTK_LABEL(label_stats), stats);
}

void filter_treeviews(SharedData *shared_data, const char *keyword) {
    gtk_list_store_clear(shared_data->store_tang1);
    gtk_list_store_clear(shared_data->store_tang2);

    GtkTreeIter iter;
    for (int i = 0; i < num_vehicles; i++) {
        if (strstr(vehicle_list[i].license_plate, keyword) != NULL) {
            if (vehicle_list[i].floor == 1) {
                gtk_list_store_append(shared_data->store_tang1, &iter);
                gtk_list_store_set(shared_data->store_tang1, &iter, 0, vehicle_list[i].license_plate, -1);
            } else if (vehicle_list[i].floor == 2) {
                gtk_list_store_append(shared_data->store_tang2, &iter);
                gtk_list_store_set(shared_data->store_tang2, &iter, 0, vehicle_list[i].license_plate, -1);
            }
        }
    }
}
static void on_search_changed(GtkEditable *entry, gpointer user_data) {
    SharedData *shared_data = (SharedData *)user_data;
    const gchar *text = gtk_entry_get_text(GTK_ENTRY(entry));
    filter_treeviews(shared_data, text);
}

int main(int argc, char **argv) {
    GtkApplication *app;
    int status;
 app = gtk_application_new("hello.world", G_APPLICATION_DEFAULT_FLAGS);
    g_signal_connect(app, "activate", G_CALLBACK(activate), NULL);
    status = g_application_run(G_APPLICATION(app), argc, argv);
    g_object_unref(app);
    return status;
}
