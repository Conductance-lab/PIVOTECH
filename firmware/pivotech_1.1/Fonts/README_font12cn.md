# Font12CN 更新说明

1. 正常情况下不需要手改 `font12cn.c`。
2. 如果你只是新增了源码里的中文界面字符串，运行：

```powershell
python Fonts/generate_font12cn.py
```

3. 如果你新增的中文来自外部配置文本，先把缺少的字符追加到 `Fonts/font12cn_extra.txt`，再重新运行生成脚本。
4. 如果删除了某些中文字符串或额外字符，同样重新运行一次脚本，未被引用的字形会自动从 `font12cn.c` 中移除。
5. 生成完成后重新编译工程即可。

生成脚本会自动：

- 扫描 `main.cpp`、`menu/`、`read/` 下活动代码里的字符串字面量
- 自动提取其中的中文和中文标点
- 直接解析 `Fonts/wenquanyi_11pt.bdf` 的原始位图，并裁成 14x14 字形
- 输出 `Fonts/font12cn.c`

当前使用的 `wenquanyi_11pt.bdf` 来自 WenQuanYi Bitmap Song 官方 BDF 包，对应 15px 版本；这里不再经过 TTF 栅格化，所以不会再引入额外的 bbox/基线偏移。
