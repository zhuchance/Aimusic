package main

import (
	"fyne.io/fyne/v2/app"
	"fyne.io/fyne/v2/container"
	"fyne.io/fyne/v2/widget"
	"os"
)
//
//func ()  {
//
//}


func main0() {
	os.Setenv("FYNE_FONT", "./wqy-microhei.ttc")
	a := app.New()
	w := a.NewWindow("标题栏")

	hello := widget.NewLabel("标签栏")
	w.SetContent(container.NewVBox(
		hello,
		widget.NewButton("按钮", func() {
			hello.SetText("文本框")
		}),
	))

	w.ShowAndRun()
	os.Unsetenv("FYNE_FONT")
}