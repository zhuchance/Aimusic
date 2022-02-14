package main

import (
	"fmt"
	"github.com/mozillazg/go-pinyin"
	"io/ioutil"
	"os"
)

func allwords() []byte {
	file, err := os.Open("song.txt")
	if err != nil {
		panic(err)
	}
	defer file.Close()
	content, err := ioutil.ReadAll(file)
	fmt.Println(string(content))
	return content
}

func getpy(strCharacter string) [][]string {
	newPy := pinyin.NewArgs()
	//下面这行代码包含声调，但在代码里不容易处理 // [[zhōng] [guó] [rén]]
	//newPy.Style = pinyin.Tone
	//使用数字表示声调，下面这行代码注释后则不显示声调 // [[zhōng] [guó] [rén]]
	newPy.Style = pinyin.Tone2
	bytpy := pinyin.Pinyin(strCharacter, newPy)
	fmt.Println(bytpy)

	return bytpy
}

func getpz() {
	//pylen := len(getpy(string(allwords())))
	py := getpy(string(allwords()))
	for i := 0; i < len(py); i++ {

		fmt.Println(py[i])
		//if strconv.Atoi(py[i]) <= 3 {
		//	fmt.Println("平声：", py[i])
		//}
		//else  {
		//	fmt.Println("仄声：", py[i])
		//}
		//intpy, err := strconv.Atoi(py[i].regexp.MustCompile("[0-9]+"))
		//fmt.Println(intpy, err, reflect.TypeOf(intpy))
	}

}

func main() {
	//getpz()
	//fmt.Println(string(allwords()))
	getpy(string(allwords()))
}

func fpy() {
	hans := "中国人"

	// 默认
	a := pinyin.NewArgs()
	fmt.Println(pinyin.Pinyin(hans, a))
	// [[zhong] [guo] [ren]]

	// 包含声调
	a.Style = pinyin.Tone
	fmt.Println(pinyin.Pinyin(hans, a))
	// [[zhōng] [guó] [rén]]

	// 声调用数字表示
	a.Style = pinyin.Tone2
	fmt.Println(pinyin.Pinyin(hans, a))
	// [[zho1ng] [guo2] [re2n]]

	// 开启多音字模式
	a = pinyin.NewArgs()
	a.Heteronym = true
	fmt.Println(pinyin.Pinyin(hans, a))
	// [[zhong zhong] [guo] [ren]]
	a.Style = pinyin.Tone2
	fmt.Println(pinyin.Pinyin(hans, a))
	// [[zho1ng zho4ng] [guo2] [re2n]]

	fmt.Println(pinyin.LazyPinyin(hans, pinyin.NewArgs()))
	// [zhong guo ren]

	fmt.Println(pinyin.Convert(hans, nil))
	// [[zhong] [guo] [ren]]

	fmt.Println(pinyin.LazyConvert(hans, nil))
	// [zhong guo ren]
}
