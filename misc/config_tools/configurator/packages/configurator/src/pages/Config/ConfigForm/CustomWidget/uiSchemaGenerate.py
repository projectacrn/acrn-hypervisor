import json


def generate_ui_schema(cur_node_name):
    def _handle(node_path_list: list):
        val = node_path_list[0]
        if val == '0':
            val = 'items'
        if len(node_path_list) == 1:
            return {
                val: {}
            }
        node_path_list = node_path_list[1:]

        return {
            val: _handle(node_path_list)
        }

    path_list = cur_node_name.split('.')
    return _handle(path_list)


def main():
    name = 'cpu_affinity.pcpu.0.pcpu_id'
    result = generate_ui_schema(name)
    print(json.dumps(result, indent='  '))


if __name__ == '__main__':
    main()
