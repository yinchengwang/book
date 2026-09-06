import { useEffect } from 'react'
import { AchievementToast } from '@/components/common/AchievementToast'
import { initStorage } from '@/utils/storage'
import './app.scss'

function App ({ children }: { children?: React.ReactNode }) {
  useEffect(() => { initStorage() }, [])
  return (
    <>
      {children}
      <AchievementToast />
    </>
  )
}

export default App