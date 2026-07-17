// Taro 入口文件
import { Component, ReactNode } from 'react'
import './styles/index.css'

interface AppProps {
  children?: ReactNode
}

class App extends Component<AppProps> {
  componentDidMount() {}

  componentDidShow() {}

  componentDidHide() {}

  render() {
    return this.props.children
  }
}

export default App
